module;
#include <cassert>
export module v8_js:platform.foreground_scheduler;
import :platform.scheduler;
import :platform.thread_cache;
import std;
import util;

namespace js::iv8::platform {

inline thread_local const void* current_foreground_scheduler = nullptr;

template <class Queue>
struct foreground_storage : scheduler_storage<Queue> {
		std::thread::id active;
		util::function_ref<auto() noexcept -> void> on_thread_exit{+[] noexcept {}};
};

// Dispatches tasks from a queue to one active foreground thread at a time. A suspended thread
// releases the queue so another thread from the cache can take over.
export template <class Queue>
class foreground_scheduler : public scheduler_of<Queue, foreground_storage<Queue>> {
	private:
		using storage_type = foreground_storage<Queue>;
		using scheduler_type = scheduler_of<Queue, storage_type>;
		friend scheduler_type;

	public:
		using scheduler_type::scheduler_type;
		foreground_scheduler(const foreground_scheduler&) = delete;
		~foreground_scheduler();
		auto operator=(const foreground_scheduler&) -> foreground_scheduler& = delete;

		[[nodiscard]] auto is_this_thread() const -> bool { return current_foreground_scheduler == this; }
		auto join() -> void;
		auto on_thread_exit(util::function_ref<auto() noexcept -> void> callback) -> void;
		auto signal(std::invocable<Queue&> auto operation) -> void;
		auto suspend(std::invocable auto operation) -> decltype(auto);

	private:
		static auto activate(storage_type& storage) -> bool;
		auto spawn(storage_type& storage) -> void;
		auto run(auto& lock, runner_thread& thread, util::inplace_stop_token stop_token) -> void;
};

// ---

template <class Queue>
foreground_scheduler<Queue>::~foreground_scheduler() {
	this->terminate();
	join();
}

template <class Queue>
auto foreground_scheduler<Queue>::join() -> void {
	assert(!is_this_thread());
	scheduler_type::join();
}

template <class Queue>
auto foreground_scheduler<Queue>::on_thread_exit(util::function_ref<auto() noexcept -> void> callback) -> void {
	this->storage().write()->on_thread_exit = callback;
}

template <class Queue>
auto foreground_scheduler<Queue>::signal(std::invocable<Queue&> auto operation) -> void {
	auto lock = this->storage().write_notify(scheduler_type::has_tasks);
	operation(lock->queue);
	if (lock->active == std::thread::id{}) {
		spawn(*lock);
	}
}

template <class Queue>
auto foreground_scheduler<Queue>::suspend(std::invocable auto operation) -> decltype(auto) {
	assert(is_this_thread());
	{
		auto lock = this->storage().write_waitable(scheduler_type::has_tasks);
		if (lock->active == std::this_thread::get_id()) {
			lock->active = {};
			if (!lock->queue.empty()) {
				spawn(*lock);
			}
		}
	}
	return operation();
}

template <class Queue>
auto foreground_scheduler<Queue>::activate(storage_type& storage) -> bool {
	const auto this_thread = std::this_thread::get_id();
	if (storage.active == std::thread::id{}) {
		storage.active = this_thread;
		return true;
	} else {
		return storage.active == this_thread;
	}
}

template <class Queue>
auto foreground_scheduler<Queue>::spawn(storage_type& storage) -> void {
	auto thread = this->request_thread(storage, thread_cache::request_kind::hard);
	assert(thread != std::thread::id{});
	storage.active = thread;
}

template <class Queue>
auto foreground_scheduler<Queue>::run(auto& lock, runner_thread& thread, util::inplace_stop_token stop_token) -> void {
	const auto scope_current_scheduler = util::scope_stack{current_foreground_scheduler, static_cast<const void*>(this)};
	while (!stop_token.stop_requested() && activate(*lock)) {
		if (auto task = this->wait_for_task(lock, thread)) {
			this->run_task(lock, *std::move(task));
		} else if (task.error()) {
			break;
		}
	}
	if (lock->active == std::this_thread::get_id()) {
		lock->active = {};
		// nb: Reassigned by the cache at the same time new work was assigned
		if (!stop_token.stop_requested() && !lock->queue.empty()) {
			spawn(*lock);
		}
	}
	lock->on_thread_exit();
	lock->on_thread_exit = +[] noexcept {};
}

} // namespace js::iv8::platform
