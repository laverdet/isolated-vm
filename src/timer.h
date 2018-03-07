#pragma once
#include <uv.h>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace ivm {

/**
 * isolated-vm could start timers from different threads which libuv isn't really cut out for, so
 * I'm rolling my own here. The goal of the library is to have atomic timers without spawning a new
 * thread for each timer.
 */
class timer_t {
	private:

		/**
		 * Contains data on a timer. This is shared between the timer handle and the thread responsible
		 * for the timer.
		 */
		struct timer_data_t {
			std::function<void()> callback;
			bool is_alive;
			bool is_running;
			bool is_dtor_waiting;
			const std::chrono::steady_clock::time_point timeout;

			timer_data_t(std::chrono::steady_clock::time_point timeout, std::function<void()> callback) :
				callback(std::move(callback)), is_alive(true), is_running(false), is_dtor_waiting(false), timeout(timeout) {
			}

			// Lock must be locked! It will be returned locked as well.
			void run(std::unique_lock<std::mutex>& lock) {
				if (is_alive) {
					is_running = true;
					lock.unlock();
					callback();
					lock.lock();
					is_running = false;
					if (is_dtor_waiting) {
						global_cv().notify_all();
					}
				}
			}

			struct cmp {
				bool operator()(const std::shared_ptr<timer_data_t>& left, const std::shared_ptr<timer_data_t>& right) const {
					return left->timeout > right->timeout;
				}
			};
		};

		/**
		 * Manager for a thread which handles 1 or many timers.
		 */
		class timer_thread_t {
			public:
				std::chrono::steady_clock::time_point next_timeout;
				std::priority_queue<
					std::shared_ptr<timer_data_t>,
					std::deque<std::shared_ptr<timer_data_t>>,
					timer_data_t::cmp
				> queue;

				explicit timer_thread_t(std::shared_ptr<timer_data_t> first_timer) :
					next_timeout(first_timer->timeout), queue(&first_timer, &first_timer + 1) {
					std::thread thread(std::bind(&timer_thread_t::entry, this));
					thread.detach();
				}

				void entry() {
					std::unique_lock<std::mutex> lock(timer_t::global_mutex(), std::defer_lock);
					while (true) {
						std::this_thread::sleep_until(next_timeout);
						lock.lock();
						std::shared_ptr<timer_data_t> current = queue.top();
						queue.pop();
						if (queue.empty()) {
							auto& threads = thread_list();
							auto ii = threads.find(this);
							if (ii == threads.end()) {
								throw std::logic_error("Didn't find thread in thread list");
							}
							threads.erase(ii);
							current->run(lock);
							delete this;
							return;
						}
						next_timeout = queue.top()->timeout;
						current->run(lock);
						lock.unlock();
					}
				}
		};

		/**
		 * Global mutex and list of active threads
		 */
		static std::mutex& global_mutex() {
			static std::mutex global_mutex;
			return global_mutex;
		}

		static std::condition_variable& global_cv() {
			static std::condition_variable cv;
			return cv;
		}

		static std::unordered_set<timer_thread_t*>& thread_list() {
			static std::unordered_set<timer_thread_t*> thread_list;
			return thread_list;
		}

		static void start_or_join_timer(std::shared_ptr<timer_data_t> data) {
			// Try to find a thread to put this timer into
			std::lock_guard<std::mutex> lock(global_mutex());
			for (auto& thread : thread_list()) {
				if (thread->next_timeout < data->timeout) {
					thread->queue.push(std::move(data));
					return;
				}
			}

			// Time to spawn a new thread
			thread_list().insert(new timer_thread_t(std::move(data)));
		}

	public:
		std::shared_ptr<timer_data_t> data;

		// Runs a callback unless the `timer_t` destructor is called.
		timer_t(uint32_t ms, std::function<void()> callback) :
			data(std::make_shared<timer_data_t>(
				std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
				std::move(callback)
			)) {
			start_or_join_timer(data);
		}
		timer_t(const timer_t&) = delete;
		timer_t& operator= (const timer_t&) = delete;

		~timer_t() {
			std::unique_lock<std::mutex> lock(global_mutex());
			if (data->is_running) {
				data->is_dtor_waiting = true;
				auto& cv = global_cv();
				do {
					cv.wait(lock);
				} while (data->is_running);
			}
			data->is_alive = false;
		}

		// Runs a callback in `ms` with no `timer_t` object.
		static void wait_detached(uint32_t ms, std::function<void()> callback) {
			start_or_join_timer(std::make_shared<timer_data_t>(
				std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
				std::move(callback)
			));
		}
};

} // namespace ivm
