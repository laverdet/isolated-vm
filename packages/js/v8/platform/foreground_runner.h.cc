module;
#include <cassert>
#include <chrono>
#include <concepts>
#include <memory>
#include <stop_token>
#include <utility>
export module v8_js:platform.foreground_runner;
import :platform.scheduler;
import :platform.task_queue;
import :platform.task_runner;
import ivm.utility;
import v8;
using std::chrono::steady_clock;

namespace js::iv8::platform {

class foreground_task_queue {
	public:
		using clock_type = std::chrono::steady_clock;
		using task_entry_type = nestable_entry<invocable_task>;
		using queue_type = util::tinker_queue<task_entry_type>;
		using delayed_queue_type = delayed_task_queue<task_entry_type>;
		using delayed_entry_type = delayed_queue_type::value_type;

		[[nodiscard]] auto empty() const -> bool { return tasks_.empty(); }
		auto flush(clock_type::time_point now) -> clock_type::time_point;
		auto front() -> task_entry_type&;
		auto pop_all() -> void;
		auto pop() -> void { tasks_.pop(); }
		auto push_delayed(delayed_entry_type task) -> void;
		auto push(v8::TaskPriority priority, task_entry_type task) -> void;

	private:
		util::segmented_priority_queue<queue_type, priority_count> tasks_;
		delayed_queue_type delayed_tasks_;
};

// Foreground task runner. Combines scheduler and task queue.
export class foreground_runner;

// Posts tasks to `foreground_task_queue` with the statically-known priority.
template <v8::TaskPriority Priority>
class foreground_runner_of_priority : public task_runner_of<foreground_runner_of_priority<Priority>> {
	public:
		auto post_delayed(task_type task, steady_clock::duration duration) -> void final;
		auto post_non_nestable_delayed(task_type task, steady_clock::duration duration) -> void;
		auto post_non_nestable(task_type task) -> void;
		auto post(task_type task) -> void final;

	private:
		auto self() -> foreground_runner& { return static_cast<foreground_runner&>(*this); }
};

// Instantiate `foreground_runner_of_priority` for each priority
template <class Sequence>
class foreground_runner_of_priorities;

template <priority_numeric_type... Priority>
class foreground_runner_of_priorities<std::integer_sequence<priority_numeric_type, Priority...>>
		: public foreground_runner_of_priority<priority_enum_from(Priority)>... {};

using foreground_runner_for_each_priority =
	foreground_runner_of_priorities<std::make_integer_sequence<priority_numeric_type, priority_count>>;

// Holds a lockable array of task queues by priority. Insertion into the queues is handled by the
// `foreground_runner_of_priority` subclasses.
export class foreground_runner : public foreground_runner_for_each_priority {
	private:
		template <v8::TaskPriority>
		friend class foreground_runner_of_priority;

		using scheduler_type = scheduler<scheduler_foreground_thread, foreground_task_queue>;

	public:
		// clears all tasks w/o executing them
		auto finalize() -> void;

		// schedule non-nestable with `kUserVisible` priority
		template <class... Args>
		auto schedule_client_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void;

		// schedule nestable with `kUserBlocking` priority, and runs immediately if this is the
		// foreground thread
		template <class... Args>
		auto schedule_handle_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void;

		// stops the foreground thread, cancelling running tasks
		auto terminate() -> void;

		// cast to `v8::TaskRunner` with the requested priority baked in
		static auto get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;

	private:
		auto scheduler() -> scheduler_type& { return scheduler_; }

		scheduler_type scheduler_;
};

// ---

// `foreground_runner_of_priority<Priority>`
template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_delayed(task_type task, steady_clock::duration duration) -> void {
	self().scheduler().signal([ & ](foreground_task_queue& storage) -> void {
		auto entry = foreground_task_queue::delayed_entry_type{steady_clock::now() + duration, nestability::nestable, std::move(task)};
		storage.push_delayed(std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_non_nestable_delayed(task_type task, steady_clock::duration duration) -> void {
	self().scheduler().signal([ & ](foreground_task_queue& storage) -> void {
		auto entry = foreground_task_queue::delayed_entry_type{steady_clock::now() + duration, nestability::non_nestable, std::move(task)};
		storage.push_delayed(std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_non_nestable(task_type task) -> void {
	self().scheduler().signal([ & ](foreground_task_queue& storage) -> void {
		auto entry = foreground_task_queue::task_entry_type{nestability::non_nestable, std::move(task)};
		storage.push(Priority, std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post(task_type task) -> void {
	self().scheduler().signal([ & ](foreground_task_queue& storage) -> void {
		auto entry = foreground_task_queue::task_entry_type{nestability::nestable, std::move(task)};
		storage.push(Priority, std::move(entry));
	});
}

// `foreground_runner`
template <class... Args>
auto foreground_runner::schedule_client_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void {
	scheduler_.signal([ & ](foreground_task_queue& storage) -> void {
		auto task = make_task_of(std::move(operation), std::forward<decltype(args)>(args)...);
		auto entry = foreground_task_queue::task_entry_type{nestability::non_nestable, std::move(task)};
		storage.push(v8::TaskPriority::kUserVisible, std::move(entry));
	});
}

template <class... Args>
auto foreground_runner::schedule_handle_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void {
	if (scheduler_.is_this_thread()) {
		operation(std::stop_token{}, std::forward<decltype(args)>(args)...);
	} else {
		scheduler_.signal([ & ](foreground_task_queue& storage) -> void {
			auto task = make_task_of(std::move(operation), std::forward<decltype(args)>(args)...);
			auto entry = foreground_task_queue::task_entry_type{nestability::nestable, std::move(task)};
			storage.push(v8::TaskPriority::kUserBlocking, std::move(entry));
		});
	}
}

} // namespace js::iv8::platform
