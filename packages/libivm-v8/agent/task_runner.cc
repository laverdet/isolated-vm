module;
#include <algorithm>
#include <chrono>
#include <experimental/scope>
#include <memory>
#include <queue>
#include <utility>
module ivm.isolated_v8;
import :agent.task_runner;
import :platform.task_runner;
import v8;

namespace ivm {

auto task_is_nestable(const std::pair<task_runner::nestability, task_runner::task_type>& task) -> bool {
	return task.first == task_runner::nestability::nestable;
}

// foreground_runner
agent::foreground_runner::foreground_runner() :
		storage_{nesting_depth_} {}

auto agent::foreground_runner::post_delayed(std::unique_ptr<v8::Task> task, steady_clock::time_point timeout) -> void {
	storage_.write()->delayed_tasks_.emplace(nestability::nestable, timeout, std::move(task));
}

auto agent::foreground_runner::post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void {
	storage_.write()->delayed_tasks_.emplace(nestability::non_nestable, timeout, std::move(task));
};

auto agent::foreground_runner::post_non_nestable(std::unique_ptr<v8::Task> task) -> void {
	storage_.write()->tasks_.emplace_back(nestability::non_nestable, std::move(task));
}

auto agent::foreground_runner::post(std::unique_ptr<v8::Task> task) -> void {
	storage_.write()->tasks_.emplace_back(nestability::nestable, std::move(task));
}

agent::foreground_runner::storage::storage(int& nesting_depth) :
		nesting_depth_{&nesting_depth} {}

auto agent::foreground_runner::storage::acquire() -> task_type {
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

auto agent::foreground_runner::storage::flush_delayed() -> void {
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

auto agent::foreground_runner::storage::should_resume() const -> bool {
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
