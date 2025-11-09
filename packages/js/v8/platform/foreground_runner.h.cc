module;
#include <array>
#include <cassert>
#include <concepts>
#include <memory>
#include <stop_token>
#include <type_traits>
export module v8_js:platform.foreground_runner;
import :platform.scheduler;
import :platform.task_runner;
import :platform.task_queue;
import ivm.utility;
import v8;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// Utilities to cast back and forth from enum type <-> numeric type
using priority_numeric_type = std::underlying_type_t<v8::TaskPriority>;

template <v8::TaskPriority Priority>
// NOLINTNEXTLINE(google-readability-casting) -- Linting is incorrect, it is not a C-style cast!
constexpr auto priority_numeric_from = static_cast<priority_numeric_type>(Priority);

template <priority_numeric_type Priority>
constexpr auto priority_enum_from = static_cast<v8::TaskPriority>(Priority);

constexpr auto priority_count = priority_numeric_from<v8::TaskPriority::kMaxPriority> + 1;

// Task queue for `foreground_runner`. There are N (priorities) of these created.
struct foreground_task_queue {
		auto clear() -> void;
		auto flush_delayed(steady_clock::time_point time_point) -> bool;
		auto pop_nestable() -> invocable_task_type;
		auto pop() -> invocable_task_type;
		auto push_delayed_task(delayed_task_entry task) -> void;
		auto push_task(task_entry task) -> void;

		task_queue tasks;
		delayed_task_queue delayed_tasks;
};

// Creates a `foreground_task_queue` for each priority
struct foreground_runner_storage {
	public:
		auto clear() -> void;
		auto flush_delayed() -> void;
		auto pop_nestable() -> invocable_task_type;
		auto pop() -> invocable_task_type;
		auto push_delayed_task(auto priority, delayed_task_entry task) -> void;
		auto push_task(auto priority, task_entry task) -> void;
		[[nodiscard]] auto should_wake() const -> bool;

	private:
		using queue_by_priority_type = std::array<foreground_task_queue, priority_count>;
		using active_queue_range_type = util::subrange_ratchet<queue_by_priority_type::iterator>;

		queue_by_priority_type queues_by_priority_;
		active_queue_range_type has_tasks_{queues_by_priority_.end(), queues_by_priority_.end()};
		active_queue_range_type has_delayed_tasks_{has_tasks_};
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
		: public foreground_runner_of_priority<priority_enum_from<Priority>>... {};

using foreground_runner_for_each_priority =
	foreground_runner_of_priorities<std::make_integer_sequence<priority_numeric_type, priority_count>>;

// Holds a lockable array of task queues by priority. Insertion into the queues is handled by the
// `foreground_runner_of_priority` subclasses.
export class foreground_runner : public foreground_runner_for_each_priority {
	private:
		template <v8::TaskPriority>
		friend class foreground_runner_of_priority;

		constexpr static auto storage_acquire_task = util::fn<&foreground_runner_storage::pop>;
		constexpr static auto storage_should_wake = util::fn<&foreground_runner_storage::should_wake>;
		using scheduler_type = scheduler<foreground_runner_storage, storage_acquire_task, storage_should_wake>;

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

		// stops the foreground thread
		auto terminate() -> void;

		// cast to `v8::TaskRunner` with the requested priority baked in
		static auto get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;

	private:
		auto scheduler() -> scheduler_type& { return scheduler_; }

		scheduler_type scheduler_;
		static thread_local std::stop_token* stop_token_;
};

// ---

// `foreground_runner_storage`
auto foreground_runner_storage::push_delayed_task(auto priority, delayed_task_entry task) -> void {
	auto queue = std::next(queues_by_priority_.begin(), priority_numeric_from<priority>);
	queue->push_delayed_task(std::move(task));
	has_delayed_tasks_.insert(queue);
}

auto foreground_runner_storage::push_task(auto priority, task_entry task) -> void {
	auto queue = std::next(queues_by_priority_.begin(), priority_numeric_from<priority>);
	queue->push_task(std::move(task));
	has_tasks_.insert(queue);
}

// `foreground_runner_of_priority<Priority>`
template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_delayed(task_type task, steady_clock::duration duration) -> void {
	self().scheduler().signal([ & ](foreground_runner_storage& storage) -> void {
		auto entry = delayed_task_entry{{.task = std::move(task), .nestability = task_entry::nestability::nestable}, steady_clock::now() + duration};
		storage.push_delayed_task(util::cw<Priority>, std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_non_nestable_delayed(task_type task, steady_clock::duration duration) -> void {
	self().scheduler().signal([ & ](foreground_runner_storage& storage) -> void {
		auto entry = delayed_task_entry{{.task = std::move(task), .nestability = task_entry::nestability::non_nestable}, steady_clock::now() + duration};
		storage.push_delayed_task(util::cw<Priority>, std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post_non_nestable(task_type task) -> void {
	self().scheduler().signal([ & ](foreground_runner_storage& storage) -> void {
		auto entry = task_entry{.task = std::move(task), .nestability = task_entry::nestability::non_nestable};
		storage.push_task(util::cw<Priority>, std::move(entry));
	});
}

template <v8::TaskPriority Priority>
auto foreground_runner_of_priority<Priority>::post(task_type task) -> void {
	self().scheduler().signal([ & ](foreground_runner_storage& storage) -> void {
		auto entry = task_entry{.task = std::move(task), .nestability = task_entry::nestability::nestable};
		storage.push_task(util::cw<Priority>, std::move(entry));
	});
}

// `foreground_runner`
template <class... Args>
auto foreground_runner::schedule_client_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void {
	scheduler_.signal([ & ](foreground_runner_storage& storage) -> void {
		auto task = make_task_of(std::move(operation), std::forward<decltype(args)>(args)...);
		auto entry = task_entry{.task = std::move(task), .nestability = task_entry::nestability::non_nestable};
		storage.push_task(util::cw<v8::TaskPriority::kUserVisible>, std::move(entry));
	});
}

template <class... Args>
auto foreground_runner::schedule_handle_task(std::invocable<std::stop_token, Args...> auto operation, Args&&... args) -> void {
	if (scheduler_.is_this_thread()) {
		operation(std::stop_token{}, std::forward<decltype(args)>(args)...);
	} else {
		scheduler_.signal([ & ](foreground_runner_storage& storage) -> void {
			auto task = make_task_of(std::move(operation), std::forward<decltype(args)>(args)...);
			auto entry = task_entry{.task = std::move(task), .nestability = task_entry::nestability::nestable};
			storage.push_task(util::cw<v8::TaskPriority::kUserBlocking>, std::move(entry));
		});
	}
}

} // namespace js::iv8::platform
