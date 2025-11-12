module;
#include <cassert>
#include <chrono>
module v8_js;
import :platform.worker_runner;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// `workers_task_queue`
auto workers_task_queue::flush(clock_type::time_point now) -> clock_type::time_point {
	return delayed_tasks_.flush(tasks_.at(priority_numeric_from(v8::TaskPriority::kUserVisible)), now);
}

auto workers_task_queue::front() -> task_entry_type& {
	return std::ranges::find_if_not(tasks_.containers(), &queue_type::empty)->front();
}

auto workers_task_queue::push_delayed(delayed_entry_type task) -> void {
	delayed_tasks_.emplace(std::move(task));
}

auto workers_task_queue::push(v8::TaskPriority priority, task_entry_type task) -> void {
	tasks_.emplace(priority_numeric_from(priority), std::move(task));
}

// `worker_runner`
auto worker_runner::post_delayed(task_type task, steady_clock::duration duration) -> void {
	scheduler_.signal([ & ](workers_task_queue& storage) -> void {
		auto entry = workers_task_queue::delayed_entry_type{steady_clock::now() + duration, std::move(task)};
		storage.push_delayed(std::move(entry));
	});
}

auto worker_runner::post(v8::TaskPriority priority, task_type task) -> void {
	scheduler_.signal([ & ](workers_task_queue& storage) -> void {
		auto entry = workers_task_queue::task_entry_type{std::move(task)};
		storage.push(priority, std::move(entry));
	});
}

} // namespace js::iv8::platform
