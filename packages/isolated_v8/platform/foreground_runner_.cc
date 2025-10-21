module;
#include <array>
#include <cassert>
#include <concepts>
#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
export module isolated_v8:foreground_runner;
import :scheduler;
import :task_runner;
import ivm.utility;
import v8;
using std::chrono::steady_clock;

namespace isolated_v8 {

// v8's `GetForegroundTaskRunner` API doesn't make any sense to me:
//
//   `std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(Isolate* isolate, TaskPriority priority)`
//
// Alright, so you want me to maintain N different shared_ptrs each of which control the same
// underlying foreground thread? In that case, what even is the meaning of a "high priority idle
// task"? So, a whacky contract gets an equally whacky implementation.

using idle_task = task_runner::idle_task;
using task_type = task_runner::task_type;
template <class Type>
concept host_task_type = std::invocable<Type, std::stop_token>;

enum class nestability : bool {
	non_nestable,
	nestable
};

// Allow lambda-style callbacks to be called with the same virtual dispatch as `v8::Task`
template <host_task_type Invocable>
class task_of final : public v8::Task {
	public:
		explicit task_of(Invocable task) :
				task_{std::move(task)} {}

		auto Run() -> void final;

	private:
		Invocable task_;
};

auto make_task_of(host_task_type auto task) -> std::unique_ptr<v8::Task> {
	return std::make_unique<task_of<decltype(task)>>(std::move(task));
}

// Holder of task + nestability
struct task {
		// nb: Mutable for use in `std::priority_queue`
		mutable task_type task;
		nestability nestability;
};

// Holder of task + timeout + nestability
struct delayed_task : task {
		struct timeout_predicate;
		auto operator<=>(const delayed_task& right) const -> std::strong_ordering;
		auto operator<=>(steady_clock::time_point right) const -> std::strong_ordering;

		steady_clock::time_point timeout;
};

// Queue of tasks
class task_queue {
	public:
		[[nodiscard]] auto empty() const -> bool;
		auto clear() -> void;
		auto pop_non_nestable() -> task_type;
		auto pop() -> task_type;
		auto push(task task) -> void;

	private:
		std::deque<task> tasks_;
		size_t empty_tasks_{};
};

// Queue of delayed tasks
class delayed_task_queue {
	public:
		[[nodiscard]] auto empty() const -> bool;
		auto clear() -> void;
		auto pop_if(std::chrono::steady_clock::time_point time) -> std::optional<task>;
		auto push(delayed_task task) -> void;

	private:
		std::priority_queue<delayed_task> tasks_;
};

// Posts tasks of the given priority to the underlying `foreground_runner` class
template <class Self, v8::TaskPriority Priority>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility) -- `Self` is not actually the immediate superclass
class foreground_runner_of_priority
		: public task_runner::task_runner_of<foreground_runner_of_priority<Self, Priority>> {
	public:
		auto post_delayed(task_type task, steady_clock::time_point timeout) -> void final;
		auto post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void;
		auto post_non_nestable(task_type task) -> void;
		auto post(task_type task) -> void final;

	private:
		auto self() -> Self& { return static_cast<Self&>(*this); }
};

// Utilities to cast back and forth from enum type <-> numeric type
using priority_numeric_type = std::underlying_type_t<v8::TaskPriority>;

template <v8::TaskPriority Priority>
// NOLINTNEXTLINE(google-readability-casting) -- Linting is incorrect, it is not a C-style cast!
constexpr auto priority_numeric_from = static_cast<priority_numeric_type>(Priority);

template <priority_numeric_type Priority>
constexpr auto priority_enum_from = static_cast<v8::TaskPriority>(Priority);

constexpr auto priority_queue_length = priority_numeric_from<v8::TaskPriority::kMaxPriority> + 1;

// Inherits from a given range of task priorities (all of them)
using priority_all_sequence =
	std::make_integer_sequence<priority_numeric_type, priority_queue_length>;

template <class Self, class Sequence>
class foreground_runner_of_priorities;

template <class Self, priority_numeric_type... Priority>
class foreground_runner_of_priorities<Self, std::integer_sequence<priority_numeric_type, Priority...>>
		: public foreground_runner_of_priority<Self, priority_enum_from<Priority>>... {
	private:
		friend Self;
		foreground_runner_of_priorities() = default;
};

// Holds a lockable array of task queues by priority. Insertion into the queues is handled by the
// `foreground_runner_of_priority` subclasses.
export class foreground_runner
		: public foreground_runner_of_priorities<foreground_runner, priority_all_sequence> {
	private:
		template <class, v8::TaskPriority Priority>
		friend class foreground_runner_of_priority;
		template <host_task_type>
		friend class task_of;

		struct task_queue_for_priority {
				task_queue tasks;
				delayed_task_queue delayed_tasks;
		};

		class storage : util::non_copyable {
			public:
				template <class, v8::TaskPriority Priority>
				friend class foreground_runner_of_priority;
				friend foreground_runner;

			protected:
				auto clear() -> void;
				auto did_push_delayed_tasks(task_queue_for_priority* iterator) -> void;
				auto did_push_tasks(task_queue_for_priority* iterator) -> void;
				auto pop_non_nestable() -> task_type;
				auto pop() -> task_type;
				template <v8::TaskPriority Priority>
				auto task_queues_by_priority(this auto& self) -> auto&;

			private:
				auto flush_delayed() -> void;
				[[nodiscard]] auto should_resume() const -> bool;

				std::array<task_queue_for_priority, priority_queue_length> queues_by_priority_;
				util::subrange_ratchet<task_queue_for_priority*> has_tasks_{&*queues_by_priority_.end(), &*queues_by_priority_.end()};
				util::subrange_ratchet<task_queue_for_priority*> has_delayed_tasks_{has_tasks_};
				std::thread::id thread_id_;
				bool has_thread_{};
		};
		using lockable_storage = util::lockable_with<storage, { .notifiable = true }>;
		using write_notify_type = lockable_storage::write_notify_type<bool (storage::*)() const>;

	public:
		explicit foreground_runner(scheduler::layer<>& scheduler);

		auto close() -> void;
		auto finalize() -> void;
		auto foreground_thread(std::stop_token stop_token) -> void;
		static auto get_for_priority(std::shared_ptr<foreground_runner> self, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		static auto schedule_client_task(std::shared_ptr<foreground_runner> self, host_task_type auto task) -> void;
		static auto schedule_handle_task(std::shared_ptr<foreground_runner> self, host_task_type auto task) -> void;

	protected:
		auto storage(this auto& self) -> lockable_storage& { return self.storage_; }

	private:
		static auto schedule(std::shared_ptr<foreground_runner> self, write_notify_type& lock) -> void;

		lockable_storage storage_;
		scheduler::runner<> scheduler_;
		static thread_local std::stop_token* stop_token_;
};

// ---

template <host_task_type Invocable>
auto task_of<Invocable>::Run() -> void {
	assert(foreground_runner::stop_token_ != nullptr);
	task_(*foreground_runner::stop_token_);
}

template <class Self, v8::TaskPriority Priority>
auto foreground_runner_of_priority<Self, Priority>::post_delayed(task_type task, steady_clock::time_point timeout) -> void {
	auto lock = self().storage().write();
	auto& queues = lock->template task_queues_by_priority<Priority>();
	queues.delayed_tasks.push(delayed_task{{.task = std::move(task), .nestability = nestability::nestable}, timeout});
	lock->did_push_delayed_tasks(&queues);
}

template <class Self, v8::TaskPriority Priority>
auto foreground_runner_of_priority<Self, Priority>::post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void {
	auto lock = self().storage().write();
	auto& queues = lock->template task_queues_by_priority<Priority>();
	queues.delayed_tasks.push(delayed_task{{.task = std::move(task), .nestability = nestability::non_nestable}, timeout});
	lock->did_push_delayed_tasks(&queues);
}

template <class Self, v8::TaskPriority Priority>
auto foreground_runner_of_priority<Self, Priority>::post_non_nestable(task_type task) -> void {
	auto lock = self().storage().write();
	auto& queues = lock->template task_queues_by_priority<Priority>();
	queues.tasks.push(isolated_v8::task{.task = std::move(task), .nestability = nestability::non_nestable});
	lock->did_push_tasks(&queues);
}

template <class Self, v8::TaskPriority Priority>
auto foreground_runner_of_priority<Self, Priority>::post(task_type task) -> void {
	auto lock = self().storage().write();
	auto& queues = lock->template task_queues_by_priority<Priority>();
	queues.tasks.push(isolated_v8::task{.task = std::move(task), .nestability = nestability::nestable});
	lock->did_push_tasks(&queues);
}

template <v8::TaskPriority Priority>
auto foreground_runner::storage::task_queues_by_priority(this auto& self) -> auto& {
	// NOLINTNEXTLINE(google-readability-casting) -- Linting is incorrect, it is not a C-style cast!
	return self.queues_by_priority_[ priority_queue_length - priority_numeric_from<Priority> - 1 ];
}

auto foreground_runner::schedule_client_task(std::shared_ptr<foreground_runner> self, host_task_type auto task) -> void {
	auto lock = self->storage_.write_notify(&storage::should_resume);
	auto& queues = lock->task_queues_by_priority<v8::TaskPriority::kUserVisible>();
	queues.tasks.push(isolated_v8::task{.task = make_task_of(std::move(task)), .nestability = nestability::non_nestable});
	lock->did_push_tasks(&queues);
	schedule(std::move(self), lock);
}

auto foreground_runner::schedule_handle_task(std::shared_ptr<foreground_runner> self, host_task_type auto task) -> void {
	auto lock = self->storage_.write_notify(&storage::should_resume);
	if (lock->thread_id_ == std::this_thread::get_id()) {
		task(std::stop_token{});
	} else {
		auto& queues = lock->template task_queues_by_priority<v8::TaskPriority::kUserBlocking>();
		queues.tasks.push(isolated_v8::task{.task = make_task_of(std::move(task)), .nestability = nestability::nestable});
		lock->did_push_tasks(&queues);
		schedule(std::move(self), lock);
	}
}

} // namespace isolated_v8
