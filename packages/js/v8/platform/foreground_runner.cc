module;
#include <cassert>
#include <chrono>
#include <memory>
#include <utility>
module v8_js;
import :platform.foreground_runner;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// `foreground_task_queue`
auto foreground_task_queue::clear() -> void {
	tasks.clear();
	delayed_tasks.clear();
}

auto foreground_task_queue::flush_delayed(steady_clock::time_point time_point) -> bool {
	while (!delayed_tasks.empty()) {
		auto task = delayed_tasks.pop_if(time_point);
		if (task) {
			tasks.push(*std::move(task));
		} else {
			return true;
		}
	}
	return false;
}

auto foreground_task_queue::pop_nestable() -> invocable_task_type {
	return invocable_task_type{tasks.pop_nestable()};
}

auto foreground_task_queue::pop() -> invocable_task_type {
	return invocable_task_type{tasks.pop()};
}

auto foreground_task_queue::push_delayed_task(delayed_task_entry task) -> void {
	delayed_tasks.push(std::move(task));
}

auto foreground_task_queue::push_task(task_entry task) -> void {
	tasks.push(std::move(task));
}

// `foreground_runner_storage`
auto foreground_runner_storage::clear() -> void {
	for (auto& queue : queues_by_priority_) {
		queue.clear();
	}
}

auto foreground_runner_storage::flush_delayed() -> void {
	if (!has_delayed_tasks_.empty()) {
		auto current_time = std::chrono::steady_clock::now();
		for (auto& queue : has_delayed_tasks_) {
			if (!queue.flush_delayed(current_time)) {
				has_delayed_tasks_.remove(&queue);
			}
		}
	}
}

auto foreground_runner_storage::pop_nestable() -> invocable_task_type {
	flush_delayed();
	for (auto& queue : has_tasks_) {
		if (!queue.tasks.empty()) {
			return queue.pop_nestable();
		}
	}
	return {};
}

auto foreground_runner_storage::pop() -> invocable_task_type {
	flush_delayed();
	for (auto& queue : has_tasks_) {
		if (queue.tasks.empty()) {
			has_tasks_.remove(&queue);
		} else {
			return queue.pop();
		}
	}
	return {};
}

auto foreground_runner_storage::should_wake() const -> bool {
	return !has_tasks_.empty();
}

// `foreground_runner`
auto foreground_runner::finalize() -> void {
	scheduler_.mutate([ & ](foreground_runner_storage& storage) -> void {
		storage.clear();
	});
}

auto foreground_runner::terminate() -> void {
	scheduler_.terminate();
}

auto foreground_runner::get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return util::template_switch(
		static_cast<priority_numeric_type>(priority),
		util::sequence<priority_count>,
		util::overloaded{
			[ & ](auto priority) -> std::shared_ptr<v8::TaskRunner> {
				using priority_runner_of = foreground_runner_of_priority<priority_enum_from<priority>>;
				return std::static_pointer_cast<priority_runner_of>(std::move(self));
			},
			[]() -> std::shared_ptr<v8::TaskRunner> {
				throw std::logic_error{"Invalid 'foreground_runner' priority"};
			},
		}
	);
}

} // namespace js::iv8::platform
