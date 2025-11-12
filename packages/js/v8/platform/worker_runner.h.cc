module;
#include <cassert>
#include <chrono>
#include <queue>
export module v8_js:platform.worker_runner;
import :platform.scheduler;
import :platform.task_queue;
import ivm.utility;
import v8;

namespace js::iv8::platform {

// Worker runner queue shared by the whole platform (multi-cluster)
class workers_task_queue {
	public:
		using clock_type = std::chrono::steady_clock;
		using task_entry_type = invocable_task;
		using queue_type = std::queue<task_entry_type>;
		using delayed_queue_type = delayed_task_queue<task_entry_type>;
		using delayed_entry_type = delayed_queue_type::value_type;

		[[nodiscard]] auto empty() const -> bool { return tasks_.empty(); }
		auto flush(clock_type::time_point now) -> clock_type::time_point;
		auto front() -> task_entry_type&;
		auto pop() -> void { tasks_.pop(); }
		auto push_delayed(delayed_entry_type task) -> void;
		auto push(v8::TaskPriority priority, task_entry_type task) -> void;

	private:
		util::segmented_priority_queue<queue_type, priority_count> tasks_;
		delayed_queue_type delayed_tasks_;
};

// Foreground task runner. Combines scheduler and task queue.
export class worker_runner {
	private:
		using scheduler_type = scheduler<scheduler_background_threads, workers_task_queue>;

	public:
		auto post_delayed(task_type task, steady_clock::duration duration) -> void;
		auto post(v8::TaskPriority priority, task_type task) -> void;

	private:
		scheduler_type scheduler_;
};

} // namespace js::iv8::platform
