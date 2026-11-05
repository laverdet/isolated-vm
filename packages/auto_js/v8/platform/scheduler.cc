export module v8_js:platform.scheduler;
import :platform.thread_cache;
import std;
import util;

namespace js::iv8::platform {

export template <class Queue>
struct scheduler_storage {
		Queue queue;
		unsigned threads = 0;
};

// Common base of `foreground_scheduler` and `worker_scheduler`. The derived class `run()` method is
// the entrypoint of spawned threads.
export template <class Queue, class Storage = scheduler_storage<Queue>>
class scheduler_of : util::non_moveable {
	public:
		explicit scheduler_of(thread_cache& cache) : cache_{cache} {}
		auto terminate() -> void { termination_stop_source_.request_stop(); }

	protected:
		using task_entry_type = Queue::task_entry_type;
		constexpr static auto has_tasks = [](const Storage& storage) -> bool { return !storage.queue.empty(); };

		[[nodiscard]] auto cache() const -> thread_cache& { return cache_; }
		[[nodiscard]] auto storage() -> auto& { return storage_; }
		[[nodiscard]] auto termination_stop_token() const -> util::inplace_stop_token { return termination_stop_source_.get_token(); }
		auto join() -> void;
		auto request_thread(this auto& self, Storage& storage, thread_cache::request_kind kind) -> std::thread::id;
		auto run_task(auto& lock, task_entry_type task) -> void;
		auto run_task(auto& lock, task_entry_type task, std::invocable<util::inplace_stop_token, task_entry_type&> auto dispatch) -> void;
		auto wait_for_task(auto& lock, runner_thread& thread) -> std::expected<task_entry_type, bool>;

	private:
		std::reference_wrapper<thread_cache> cache_;
		util::inplace_stop_source termination_stop_source_;
		util::lockable_with<Storage, {.interuptable = true, .notifiable = true}> storage_;
};

// ---

template <class Queue, class Storage>
auto scheduler_of<Queue, Storage>::join() -> void {
	constexpr auto predicate = [](const Storage& storage) -> bool {
		return storage.threads == 0;
	};
	storage_.write_waitable(predicate).wait();
}

template <class Queue, class Storage>
auto scheduler_of<Queue, Storage>::request_thread(this auto& self, Storage& storage, thread_cache::request_kind kind) -> std::thread::id {
	auto thread = self.cache().request(kind, [ &self ](runner_thread& thread) -> void {
		auto lock = self.storage().write_waitable(has_tasks);
		self.run(lock, thread, self.termination_stop_token());
		--lock->threads;
		lock.notify_all();
	});
	if (thread != std::thread::id{}) {
		++storage.threads;
	}
	return thread;
}

template <class Queue, class Storage>
auto scheduler_of<Queue, Storage>::wait_for_task(auto& lock, runner_thread& thread) -> std::expected<task_entry_type, bool> {
	if (thread.reassigned()) {
		return std::unexpected{true};
	}
	auto timeout = lock->queue.flush(Queue::clock_type::now());
	auto pop = [ & ] -> task_entry_type {
		auto task = std::move(lock->queue.front());
		lock->queue.pop();
		return task;
	};
	if (!lock->queue.empty()) {
		return pop();
	}
	auto suspension = thread.will_suspend();
	auto any_stop_source = util::composite_stop_source{termination_stop_token(), suspension.get_token()};
	if (lock.wait(any_stop_source.get_token(), timeout)) {
		return pop();
	} else {
		return std::unexpected{suspension.reclaimed()};
	}
}

template <class Queue, class Storage>
auto scheduler_of<Queue, Storage>::run_task(auto& lock, task_entry_type task) -> void {
	run_task(lock, std::move(task), [](util::inplace_stop_token token, task_entry_type& task) { task(token); });
}

template <class Queue, class Storage>
auto scheduler_of<Queue, Storage>::run_task(auto& lock, task_entry_type task, std::invocable<util::inplace_stop_token, task_entry_type&> auto dispatch) -> void {
	lock.unlock();
	{
		// nb: The task may own the last reference to an agent. Destruction of the of the agent would
		// call back into the scheduler.
		auto unlocked_task = std::move(task);
		dispatch(termination_stop_token(), unlocked_task);
	}
	lock.lock();
}

} // namespace js::iv8::platform
