module;
#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <queue>
#include <ranges>
#include <stop_token>
#include <utility>
module isolated_v8.foreground_runner;
import isolated_v8.task_runner;
import v8;

namespace isolated_v8 {

// delayed_task
auto delayed_task::operator<=>(const delayed_task& right) const -> std::strong_ordering {
	return timeout <=> right.timeout;
}

auto delayed_task::operator<=>(steady_clock::time_point right) const -> std::strong_ordering {
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

auto task_queue::pop_non_nestable() -> task_type {
	assert(!empty());
	auto it = std::ranges::find_if(tasks_, [](const auto& task) {
		return task.nestability == nestability::nestable && task.task;
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

auto task_queue::push(task task) -> void {
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

auto delayed_task_queue::pop_if(std::chrono::steady_clock::time_point time) -> std::optional<task> {
	assert(!empty());
	const auto& top = tasks_.top();
	if (top <= time) {
		task result{.task = std::move(top.task), .nestability = top.nestability};
		tasks_.pop();
		return result;
	}
	return {};
}

auto delayed_task_queue::push(delayed_task task) -> void {
	assert(task.task);
	tasks_.emplace(std::move(task));
}

// foreground_runner
thread_local std::stop_token* foreground_runner::stop_token_{};

foreground_runner::foreground_runner(scheduler::layer<>& scheduler) :
		scheduler_{scheduler} {}

auto foreground_runner::close() -> void {
	scheduler_.close_threads();
}

auto foreground_runner::finalize() -> void {
	storage_.write()->clear();
}

auto foreground_runner::foreground_thread(std::stop_token stop_token) -> void {
	stop_token_ = &stop_token;
	auto lock = storage_.write_waitable(&storage::should_resume);
	lock->thread_id_ = std::this_thread::get_id();
	while (!stop_token.stop_requested()) {
		auto task = lock->pop();
		if (task) {
			lock.unlock();
			task->Run();
			lock.lock();
		} else {
			break;
		}
	};
	lock->has_thread_ = false;
	stop_token_ = nullptr;
}

auto foreground_runner::get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	auto pick =
		[ & ]<priority_numeric_type Priority>(
			const std::integral_constant<priority_numeric_type, Priority> /*priority*/,
			const auto& pick
		) -> std::shared_ptr<v8::TaskRunner> {
		constexpr auto priority_enum = priority_enum_from<Priority>;
		using priority_runner = foreground_runner_of_priority<foreground_runner, priority_enum>;
		if (priority == priority_enum) {
			return std::static_pointer_cast<priority_runner>(std::move(self));
		} else {
			if constexpr (priority_enum == v8::TaskPriority::kMaxPriority) {
				std::unreachable();
			} else {
				return pick(std::integral_constant<priority_numeric_type, Priority + 1>{}, pick);
			}
		}
	};
	return pick(std::integral_constant<priority_numeric_type, 0>{}, pick);
}

auto foreground_runner::schedule(std::shared_ptr<foreground_runner> self, write_notify_type& lock) -> void {
	if (!lock->has_thread_) {
		lock->has_thread_ = true;
		self->scheduler_([ self = std::move(self) ](const std::stop_token& stop_token) mutable {
			self->foreground_thread(stop_token);
			return std::move(self);
		});
	}
}

// foreground_runner::storage
auto foreground_runner::storage::clear() -> void {
	for (auto& queue : queues_by_priority_) {
		queue.delayed_tasks.clear();
		queue.tasks.clear();
	}
}

auto foreground_runner::storage::did_push_tasks(task_queue_for_priority* iterator) -> void {
	has_tasks_.insert(iterator);
}

auto foreground_runner::storage::did_push_delayed_tasks(task_queue_for_priority* iterator) -> void {
	has_delayed_tasks_.insert(iterator);
}

auto foreground_runner::storage::flush_delayed() -> void {
	if (!std::ranges::empty(has_delayed_tasks_)) {
		auto current_time = std::chrono::steady_clock::now();
		for (auto& queue : has_delayed_tasks_) {
			while (true) {
				if (queue.delayed_tasks.empty()) {
					has_delayed_tasks_.remove(&queue);
					break;
				}
				auto task = queue.delayed_tasks.pop_if(current_time);
				if (task) {
					queue.tasks.push(*std::move(task));
				} else {
					break;
				}
			}
		}
	}
}

auto foreground_runner::storage::pop() -> task_type {
	flush_delayed();
	for (auto& queue : has_tasks_) {
		if (queue.tasks.empty()) {
			has_tasks_.remove(&queue);
		} else {
			return queue.tasks.pop();
		}
	}
	return {};
}

auto foreground_runner::storage::pop_non_nestable() -> task_type {
	flush_delayed();
	for (auto& queue : has_tasks_) {
		if (!queue.tasks.empty()) {
			return queue.tasks.pop_non_nestable();
		}
	}
	return {};
}

auto foreground_runner::storage::should_resume() const -> bool {
	return !std::ranges::empty(has_tasks_);
}

} // namespace isolated_v8
