#pragma once
#include <uv.h>

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

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
			const std::chrono::steady_clock::time_point timeout;

			timer_data_t(std::chrono::steady_clock::time_point timeout, std::function<void()> callback) :
				callback(std::move(callback)), is_alive(true), timeout(timeout) {
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
				std::thread thread;

				timer_thread_t(std::shared_ptr<timer_data_t> first_timer) :
					next_timeout(first_timer->timeout), queue(&first_timer, &first_timer + 1),
					thread(std::bind(&timer_thread_t::entry, this)) {
					thread.detach();
				}

				void entry() {
					std::unique_lock<std::mutex> lock(timer_t::global_mutex(), std::defer_lock);
					while (true) {
						std::this_thread::sleep_until(next_timeout);
						lock.lock();
						std::shared_ptr<timer_data_t> current = queue.top();
						queue.pop();
						if (current->is_alive) {
							current->callback();
						}
						if (queue.empty()) {
							for (auto ii = thread_list().begin(); ii != thread_list().end(); ++ii) {
								if (ii->get() == this) {
									thread_list().erase(ii);
									return;
								}
							}
							throw std::runtime_error("Didn't find thread in thread list");
						}
						next_timeout = queue.top()->timeout;
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

		static std::deque<std::unique_ptr<timer_thread_t>>& thread_list() {
			static std::deque<std::unique_ptr<timer_thread_t>> thread_list;
			return thread_list;
		}

	public:
		std::shared_ptr<timer_data_t> data;

		timer_t(uint32_t ms, std::function<void()> callback) :
			data(std::make_shared<timer_data_t>(
				std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
				std::move(callback)
			)) {

			// Need to find a thread to put this timer into
			std::lock_guard<std::mutex> lock(global_mutex());
			for (auto& thread : thread_list()) {
				if (thread->next_timeout < data->timeout) {
					thread->queue.push(data);
					return;
				}
			}

			// Time to spawn a new thread
			thread_list().emplace_back(std::make_unique<timer_thread_t>(data));
		}

		~timer_t() {
			std::lock_guard<std::mutex> lock(global_mutex());
			data->is_alive = false;
		}

};

}
