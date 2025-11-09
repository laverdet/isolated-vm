module;
#include <atomic>
#include <concepts>
#include <stop_token>
#include <thread>
#include <utility>
export module v8_js:platform.scheduler;
import ivm.utility;

namespace js::iv8::platform {

// Storage for `scheduler`, wrapped in `util::lockable`
template <class Queue, auto Acquire, auto Predicate>
class scheduler_unlocked_storage {
	public:
		using queue_type = Queue;

		static_assert(requires(Queue storage, std::stop_token stop_token) {
			{ Predicate(storage) } -> std::same_as<bool>;
			{ bool(Acquire(storage)) } -> std::same_as<bool>;
			{ Acquire(storage)(stop_token) } -> std::same_as<void>;
		});

		[[nodiscard]] auto acquire() -> auto { return Acquire(queue_); }
		[[nodiscard]] auto predicate() const -> bool { return Predicate(queue_); }
		[[nodiscard]] auto queue() & -> auto& { return queue_; };
		[[nodiscard]] auto thread() & -> auto& { return thread_; };

	private:
		Queue queue_;
		std::jthread thread_;
};

// Implementation of `scheduler`
template <class Storage>
class scheduler_with : util::non_copyable {
	public:
		using queue_type = Storage::queue_type;

		[[nodiscard]] auto is_this_thread() const -> bool;

		auto mutate(std::invocable<queue_type&> auto operation) -> auto;

		template <class... Args>
		auto signal(std::invocable<queue_type&, Args...> auto operation, Args&&... args) -> void;

		auto terminate() -> void;

	private:
		static auto run(std::stop_token stop_token, scheduler_with& self) -> void;

		std::atomic<std::thread::id> thread_id_;
		util::lockable_with<Storage, {.interuptable = true, .notifiable = true}> storage_;
};

// Scheduler, delegating to `scheduler_with`
export template <class Queue, auto Acquire, auto Predicate>
class scheduler : public scheduler_with<scheduler_unlocked_storage<Queue, Acquire, Predicate>> {};

// ---

template <class Storage>
auto scheduler_with<Storage>::is_this_thread() const -> bool {
	return std::this_thread::get_id() == thread_id_.load(std::memory_order_relaxed);
}

template <class Storage>
auto scheduler_with<Storage>::mutate(std::invocable<queue_type&> auto operation) -> auto {
	return operation(storage_.write()->queue());
}

template <class Storage>
template <class... Args>
auto scheduler_with<Storage>::signal(std::invocable<queue_type&, Args...> auto operation, Args&&... args) -> void {
	auto lock = storage_.write_notify(&Storage::predicate);
	operation(lock->queue(), std::forward<Args>(args)...);
	if (!lock->thread().joinable()) {
		lock->thread() = std::jthread{&scheduler_with::run, std::ref(*this)};
		thread_id_.store(lock->thread().get_id(), std::memory_order_relaxed);
	}
}

template <class Storage>
auto scheduler_with<Storage>::run(std::stop_token stop_token, scheduler_with& self) -> void {
	auto lock = self.storage_.write_waitable(&Storage::predicate);
	while (lock.wait(stop_token)) {
		auto task = lock->acquire();
		lock.unlock();
		if (task) {
			task(stop_token);
		}
		lock.lock();
	}
}

template <class Storage>
auto scheduler_with<Storage>::terminate() -> void {
	auto thread = std::exchange(storage_.write()->thread(), {});
}

} // namespace js::iv8::platform
