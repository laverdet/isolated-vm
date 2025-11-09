module;
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <queue>
#include <utility>
module v8_js;
import :platform.task_queue;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// delayed_task_entry
auto delayed_task_entry::operator<=>(const delayed_task_entry& right) const -> std::strong_ordering {
	return timeout <=> right.timeout;
}

auto delayed_task_entry::operator<=>(steady_clock::time_point right) const -> std::strong_ordering {
	return timeout <=> right;
}

// task_queue
auto task_queue::clear() -> void {
	tasks_.clear();
}

auto task_queue::empty() const -> bool {
	return tasks_.size() - empty_tasks_ == 0;
}

auto task_queue::pop() -> task_type {
	assert(!empty());
	while (true) {
		auto result = std::move(tasks_.front().task);
		tasks_.pop_front();
		if (result) {
			return result;
		} else {
			--empty_tasks_;
		}
	}
}

auto task_queue::pop_nestable() -> task_type {
	assert(!empty());
	auto it = std::ranges::find_if(tasks_, [](const auto& task) {
		return task.nestability == task_entry::nestability::nestable && task.task;
	});
	if (it != tasks_.end()) {
		auto result = std::move(it->task);
		if (it == tasks_.begin()) {
			tasks_.pop_front();
		} else {
			++empty_tasks_;
		}
		return result;
	}
	return {};
}

auto task_queue::push(task_entry task) -> void {
	assert(task.task);
	tasks_.emplace_back(std::move(task));
}

// delayed_task_queue
auto delayed_task_queue::clear() -> void {
	tasks_ = {};
}

auto delayed_task_queue::empty() const -> bool {
	return tasks_.empty();
}

auto delayed_task_queue::pop_if(steady_clock::time_point time) -> std::optional<task_entry> {
	assert(!empty());
	const auto& top = tasks_.top();
	if (top <= time) {
		auto result = task_entry{.task = std::move(top.task), .nestability = top.nestability};
		tasks_.pop();
		return result;
	}
	return {};
}

auto delayed_task_queue::push(delayed_task_entry task) -> void {
	assert(task.task);
	tasks_.emplace(std::move(task));
}

} // namespace js::iv8::platform
