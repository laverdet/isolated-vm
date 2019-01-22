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
		using callback_t = std::function<void(void*)>;

		/**
		 * Contains data on a timer. This is shared between the timer handle and the thread responsible
		 * for the timer.
		 */
		struct timer_data_t {
			callback_t callback;
			void** holder = nullptr;
			std::chrono::steady_clock::time_point timeout;
			std::chrono::steady_clock::time_point paused_at{};
			std::chrono::steady_clock::duration paused_duration{};
			std::shared_ptr<timer_data_t> threadless_self;
			bool is_alive = true;
			bool is_running = false;
			bool is_dtor_waiting = false;

			timer_data_t(std::chrono::steady_clock::time_point timeout, void** holder, callback_t callback) :
				callback(std::move(callback)), holder(holder), timeout(timeout) {
				if (holder != nullptr) {
					*holder = static_cast<void*>(this);
				}
			}

			bool adjust() {
				if (paused_duration == std::chrono::steady_clock::duration{}) {
					return false;
				} else {
					timeout += paused_duration;
					paused_duration = {};
					return true;
				}
			}

			bool is_paused() {
				return paused_at != std::chrono::steady_clock::time_point{};
			}

			void pause() {
				paused_at = std::chrono::steady_clock::now();
			}

			void resume() {
				paused_duration += std::chrono::steady_clock::now() - paused_at;
				paused_at = {};
			}

			struct cmp {
				bool operator()(const std::shared_ptr<timer_data_t>& left, const std::shared_ptr<timer_data_t>& right) const {
					return left->timeout > right->timeout;
				}
			};
		};
		static void run(std::shared_ptr<timer_data_t> data, std::unique_lock<std::mutex>& lock, class timer_thread_t& thread);

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
						next_timeout = std::chrono::steady_clock::now();
						run_next(lock);
						if (queue.empty()) {
							auto& threads = thread_list();
							auto ii = threads.find(this);
							if (ii == threads.end()) {
								throw std::logic_error("Didn't find thread in thread list");
							}
							threads.erase(ii);
							delete this;
							return;
						}
						next_timeout = queue.top()->timeout;
						lock.unlock();
					}
				}

				void run_next(std::unique_lock<std::mutex>& lock) {
					auto data = queue.top();
					queue.pop();
					run(std::move(data), lock, *this);
				}

				void maybe_run_next(std::unique_lock<std::mutex>& lock) {
					if (!queue.empty() && queue.top()->timeout <= next_timeout) {
						run_next(lock);
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

		// Lock must be locked!
		static void start_or_join_timer(std::shared_ptr<timer_data_t> data) {
			// Try to find a thread to put this timer into
			for (auto& thread : thread_list()) {
				if (thread->next_timeout < data->timeout) {
					thread->queue.push(std::move(data));
					return;
				}
			}

			// Time to spawn a new thread
			thread_list().insert(new timer_thread_t(std::move(data)));
		}

		// Lock must be locked! It will be returned locked as well.
		static void run(std::shared_ptr<timer_data_t> data, std::unique_lock<std::mutex>& lock, timer_thread_t& thread) {
			if (data->is_alive) {
				if (data->is_paused()) {
					data->threadless_self = std::move(data);
				} else if (data->adjust()) {
					start_or_join_timer(std::move(data));
				} else {
					data->is_running = true;
					lock.unlock();
					data->callback(reinterpret_cast<void*>(&thread));
					lock.lock();
					data->is_running = false;
					if (data->is_dtor_waiting) {
						global_cv().notify_all();
					}
					return;
				}
			} else {
				data.reset();
			}
			thread.maybe_run_next(lock);
		}

	public:
		std::shared_ptr<timer_data_t> data;

		// Runs a callback unless the `timer_t` destructor is called.
		timer_t(uint32_t ms, void** holder, callback_t callback) {
			std::lock_guard<std::mutex> lock(global_mutex());
			data = std::make_shared<timer_data_t>(
				std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
				holder, std::move(callback)
			);
			start_or_join_timer(data);
		}
		timer_t(uint32_t ms, callback_t callback) : timer_t(ms, nullptr, std::move(callback)) {}
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
			if (data->holder != nullptr) {
				*data->holder = nullptr;
			}
		}

		// Runs a callback in `ms` with no `timer_t` object.
		static void wait_detached(uint32_t ms, callback_t callback) {
			std::lock_guard<std::mutex> lock(global_mutex());
			start_or_join_timer(std::make_shared<timer_data_t>(
				std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
				nullptr, std::move(callback)
			));
		}

		// Invoked from callbacks when they are done scheduling and may need to wait
		static void chain(void* ptr) {
			auto& thread = *reinterpret_cast<timer_thread_t*>(ptr);
			std::unique_lock<std::mutex> lock(global_mutex());
			thread.maybe_run_next(lock);
		}

		static void pause(void*& holder) {
			std::lock_guard<std::mutex> lock(global_mutex());
			if (holder != nullptr) {
				auto& data = *static_cast<timer_data_t*>(holder);
				data.pause();
			}
		}

		static void resume(void*& holder) {
			std::lock_guard<std::mutex> lock(global_mutex());
			if (holder != nullptr) {
				auto& data = *static_cast<timer_data_t*>(holder);
				data.resume();
				if (data.threadless_self) {
					start_or_join_timer(std::move(data.threadless_self));
				}
			}
		}
};

} // namespace ivm
