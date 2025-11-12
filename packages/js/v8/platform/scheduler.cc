module;
#include <atomic>
#include <concepts>
#include <stop_token>
#include <thread>
#include <utility>
export module v8_js:platform.scheduler;
import ivm.utility;

namespace js::iv8::platform {

// Manager of one foreground thread. This outlives `controller`.
class scheduler_foreground_thread {
	public:
		class controller;
		[[nodiscard]] auto is_this_thread() const -> bool;

	protected:
		template <class Storage>
		auto signal_thread(std::stop_token termination_stop_token, Storage& unlocked_storage, Storage::resource_type& storage) -> void;

	private:
		std::atomic<std::thread::id> thread_id_;
};

// Locked storage for `scheduler_foreground_thread`
class scheduler_foreground_thread::controller {
	public:
		// TODO: Messy
		auto take() -> std::jthread { return std::move(thread_); };

		template <class Storage>
		auto signal(std::stop_token termination_stop_token, Storage& unlocked_storage) -> std::thread::id;

	private:
		template <class Storage>
		static auto run(std::stop_token stop_token, std::stop_token termination_stop_token, Storage& storage) -> void;

		std::jthread thread_;
};

// Manager of multiple background threads.
class scheduler_background_threads {
	public:
		class controller;

	protected:
		template <class Storage>
		auto signal_thread(std::stop_token termination_stop_token, Storage& unlocked_storage, Storage::resource_type& storage) -> void;
};

// Locked storage for `scheduler_background_threads`
class scheduler_background_threads::controller {
	public:
		template <class Storage>
		auto signal(std::stop_token termination_stop_token, Storage& unlocked_storage) -> void;

	private:
		template <class Storage>
		static auto run(std::stop_token stop_token, std::stop_token termination_stop_token, Storage& storage) -> void;

		std::array<std::jthread, 2> threads_;
		bool initialized_ = false;
};

// Accepts a thread controller and task queue types, and manages dispatch of events to threads.
template <class Thread, class Queue>
class scheduler : public Thread {
	private:
		using Thread::signal_thread;

		struct storage {
				using queue_type = Queue;

				Thread::controller controller;
				Queue queue;
		};

	public:
		// TODO: Messy
		auto invoke(std::invocable<typename Thread::controller&> auto operation) -> auto;

		// Perform `operation` on the queue
		auto mutate(std::invocable<Queue&> auto operation) -> auto;

		// Perform `operation` on the queue, and signal the controller if `!empty()`
		template <class... Args>
		auto signal(std::invocable<Queue&, Args...> auto operation) -> void;

		// Request immediate termination
		auto terminate() -> void { termination_stop_source_.request_stop(); }

	private:
		util::lockable_with<storage, {.interuptable = true, .notifiable = true}> storage_;
		util::scope_stop_source termination_stop_source_;
};

// ---

// `scheduler_foreground_thread`
auto scheduler_foreground_thread::is_this_thread() const -> bool {
	return std::this_thread::get_id() == thread_id_.load(std::memory_order_relaxed);
}

template <class Storage>
auto scheduler_foreground_thread::signal_thread(
	std::stop_token termination_stop_token,
	Storage& unlocked_storage,
	Storage::resource_type& storage
) -> void {
	if (auto id = storage.controller.signal(std::move(termination_stop_token), unlocked_storage); id != std::thread::id{}) {
		thread_id_.store(id, std::memory_order_relaxed);
	}
}

// `scheduler_foreground_thread::controller`
template <class Storage>
auto scheduler_foreground_thread::controller::signal(
	std::stop_token termination_stop_token,
	Storage& unlocked_storage
) -> std::thread::id {
	if (thread_.joinable()) {
		return {};
	} else {
		thread_ = std::jthread{&controller::run<Storage>, std::move(termination_stop_token), std::ref(unlocked_storage)};
		return thread_.get_id();
	}
}

template <class Storage>
auto scheduler_foreground_thread::controller::run(
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	std::stop_token stop_token,
	std::stop_token termination_stop_token,
	Storage& storage
) -> void {
	using queue_type = Storage::resource_type::queue_type;
	auto lock = storage.write_waitable([](const Storage::resource_type& storage) -> bool {
		return !storage.queue.empty();
	});
	util::composite_stop_token any_stop_token{std::move(stop_token), termination_stop_token};
	while (!any_stop_token.stop_requested()) {
		auto [... timeout ] = [ & ]() {
			if constexpr (requires { typename queue_type::clock_type; }) {
				return std::tuple{lock->queue.flush(queue_type::clock_type::now())};
			} else {
				return std::tuple{};
			}
		}();
		while (lock.wait(any_stop_token, timeout...)) {
			auto task = std::move(lock->queue.front());
			lock->queue.pop();
			lock.unlock();
			task(termination_stop_token);
			lock.lock();
		}
	}
}

// `scheduler_background_threads`
template <class Storage>
auto scheduler_background_threads::signal_thread(
	std::stop_token termination_stop_token,
	Storage& unlocked_storage,
	Storage::resource_type& storage
) -> void {
	storage.controller.signal(std::move(termination_stop_token), unlocked_storage);
}

// `scheduler_background_threads::controller`
template <class Storage>
auto scheduler_background_threads::controller::signal(
	std::stop_token termination_stop_token,
	Storage& unlocked_storage
) -> void {
	if (!initialized_) {
		initialized_ = true;
		for (auto& thread : threads_) {
			thread = std::jthread{&controller::run<Storage>, termination_stop_token, std::ref(unlocked_storage)};
		}
	}
}

template <class Storage>
auto scheduler_background_threads::controller::run(
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	std::stop_token stop_token,
	std::stop_token termination_stop_token,
	Storage& storage
) -> void {
	using queue_type = Storage::resource_type::queue_type;
	auto lock = storage.write_waitable([](const Storage::resource_type& storage) -> bool {
		return !storage.queue.empty();
	});
	util::composite_stop_token any_stop_token{std::move(stop_token), termination_stop_token};
	while (!any_stop_token.stop_requested()) {
		auto [... timeout ] = [ & ]() {
			if constexpr (requires { typename queue_type::clock_type; }) {
				return std::tuple{lock->queue.flush(queue_type::clock_type::now())};
			} else {
				return std::tuple{};
			}
		}();
		while (lock.wait(any_stop_token, timeout...)) {
			auto task = std::move(lock->queue.front());
			lock->queue.pop();
			lock.unlock();
			task(termination_stop_token);
			lock.lock();
		}
	}
}

// `scheduler`
template <class Thread, class Queue>
auto scheduler<Thread, Queue>::invoke(std::invocable<typename Thread::controller&> auto operation) -> auto {
	return operation(storage_.write()->controller);
}

template <class Thread, class Queue>
auto scheduler<Thread, Queue>::mutate(std::invocable<Queue&> auto operation) -> auto {
	return operation(storage_.write()->queue);
}

template <class Thread, class Queue>
template <class... Args>
auto scheduler<Thread, Queue>::signal(std::invocable<Queue&, Args...> auto operation) -> void {
	auto lock = storage_.write_notify([](const storage& storage) -> bool {
		return !storage.queue.empty();
	});
	operation(lock->queue);
	signal_thread(termination_stop_source_.get_token(), storage_, *lock);
}

} // namespace js::iv8::platform
