module;
#include <algorithm>
#include <chrono>
#include <memory>
#include <queue>
#include <utility>
module ivm.isolated_v8;
import :platform.foreground_runner;
import :platform.task_runner;
import v8;

namespace ivm {

auto task_is_nestable(const std::pair<task_runner::nestability, task_runner::task_type>& task) -> bool {
	return task.first == task_runner::nestability::nestable;
}

foreground_runner::foreground_runner() :
		storage_{nesting_depth_} {}

auto foreground_runner::post_delayed(task_type task, steady_clock::time_point timeout) -> void {
	storage_.write()->delayed_tasks_.emplace(nestability::nestable, timeout, std::move(task));
}

auto foreground_runner::post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void {
	storage_.write()->delayed_tasks_.emplace(nestability::non_nestable, timeout, std::move(task));
};

auto foreground_runner::post_non_nestable(task_type task) -> void {
	storage_.write()->tasks_.emplace_back(nestability::non_nestable, std::move(task));
}

auto foreground_runner::post(task_type task) -> void {
	storage_.write()->tasks_.emplace_back(nestability::nestable, std::move(task));
}

auto foreground_runner::schedule_non_nestable(task_type task) -> void {
	auto lock = storage_.write_notify();
	lock->tasks_.emplace_back(task_runner::nestability::non_nestable, std::move(task));
	lock->should_resume_ = true;
}

foreground_runner::storage::storage(int& nesting_depth) :
		nesting_depth_{&nesting_depth} {}

auto foreground_runner::storage::acquire() -> task_type {
	flush_delayed();
	if (*nesting_depth_ < 1) {
		std::unreachable();
	} else if (*nesting_depth_ == 1) {
		if (!tasks_.empty()) {
			auto result = std::move(tasks_.front().second);
			tasks_.pop_front();
			return result;
		}
	} else {
		auto it = std::ranges::find_if(tasks_, task_is_nestable);
		if (it != tasks_.end()) {
			auto result = std::move(it->second);
			tasks_.erase(it);
			return result;
		}
	}
	return {};
}

auto foreground_runner::storage::flush_delayed() -> void {
	if (!delayed_tasks_.empty()) {
		auto predicate = delayed_task::timeout_predicate();
		do {
			const auto& top = delayed_tasks_.top();
			if (predicate(top)) {
				tasks_.emplace_back(top.nestability_, std::move(top.task_));
				delayed_tasks_.pop();
			}
		} while (!delayed_tasks_.empty());
	}
}

auto foreground_runner::storage::should_resume() const -> bool {
	// Check current tasks
	if (*nesting_depth_ < 1) {
		std::unreachable();
	} else if (*nesting_depth_ == 1) {
		if (!tasks_.empty()) {
			return true;
		}
	} else {
		if (std::ranges::any_of(tasks_, task_is_nestable)) {
			return true;
		}
	}
	// Also check expiring delayed tasks
	if (!delayed_tasks_.empty()) {
		auto predicate = delayed_task::timeout_predicate();
		if (predicate(delayed_tasks_.top())) {
			return true;
		}
	}
	return false;
}

} // namespace ivm
