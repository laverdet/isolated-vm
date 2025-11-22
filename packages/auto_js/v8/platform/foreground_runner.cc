module;
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <stop_token>
#include <utility>
module v8_js;
import :platform.foreground_runner;

namespace js::iv8::platform {

// `foreground_task_queue`
foreground_task_queue::foreground_task_queue(foreground_task_queue&& other) noexcept :
		tasks_{std::move(other.tasks_)},
		delayed_tasks_{std::move(other.delayed_tasks_)},
		finalized_{other.finalized_} {
	other.finalized_ = true;
}

auto foreground_task_queue::flush(clock_type::time_point now) -> clock_type::time_point {
	// nb: As far as I can tell all delayed tasks are posted with `kUserVisible`. I'm not even sure
	// what a high priority delayed task would mean.
	return delayed_tasks_.flush(tasks_.at(priority_numeric_from(v8::TaskPriority::kUserVisible)), now);
}

auto foreground_task_queue::finalize() -> void {
	finalized_ = true;
	auto& handle_tasks = tasks_.at(priority_numeric_from(v8::TaskPriority::kUserBlocking));
	while (!handle_tasks.empty()) {
		auto& task = tasks_.front();
		task(std::stop_token{});
		tasks_.pop();
	};
	while (!tasks_.empty()) tasks_.pop();
	while (!delayed_tasks_.empty()) delayed_tasks_.pop();
}

auto foreground_task_queue::front() -> task_entry_type& {
	return std::ranges::find_if_not(tasks_.containers(), &queue_type::empty)->front();
}

auto foreground_task_queue::push_delayed(delayed_entry_type task) -> void {
	if (!finalized_) {
		delayed_tasks_.emplace(std::move(task));
	}
}

auto foreground_task_queue::push(v8::TaskPriority priority, task_entry_type task) -> void {
	if (!finalized_) {
		tasks_.emplace(priority_numeric_from(priority), std::move(task));
	}
}

// `foreground_runner`
auto foreground_runner::terminate() -> void {
	scheduler_.terminate();
}

auto foreground_runner::get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return util::template_switch(
		priority_numeric_from(priority),
		util::sequence<priority_count>,
		util::overloaded{
			[ & ](auto priority) -> std::shared_ptr<v8::TaskRunner> {
				using priority_runner_of = foreground_runner_of_priority<priority_enum_from(priority)>;
				return std::static_pointer_cast<priority_runner_of>(std::move(self));
			},
			[]() -> std::shared_ptr<v8::TaskRunner> {
				throw std::logic_error{"Invalid 'foreground_runner' priority"};
			},
		}
	);
}

} // namespace js::iv8::platform
