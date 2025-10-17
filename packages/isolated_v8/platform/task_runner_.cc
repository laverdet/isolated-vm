module;
#include <chrono>
#include <memory>
#include <utility>
export module isolated_v8:task_runner;
import v8;
using std::chrono::steady_clock;

// NOTE:
// There is nothing interesting in this file. The main thing is that `task_runner_of` automatically
// defines `NonNestableTasksEnabled` (etc) for you based on whether or not `post_non_nestable` (etc)
// is defined.

namespace isolated_v8 {

// Instances of `v8::TaskRunner` can directly accept scheduled tasks from v8, which may or may not
// run at some point in the future.
// nb: v8's documentation says "All tasks posted to a given TaskRunner will be invoked in sequence"
// but their default implementation will skip non-nestable tasks in a nestable context.
// Additionally, idle tasks are their own queue.
export class task_runner : public v8::TaskRunner {
	public:
		template <class Self>
		class task_runner_of;

		using idle_task = std::unique_ptr<v8::IdleTask>;
		using task_type = std::unique_ptr<v8::Task>;

		auto post_idle(idle_task task) -> void;
		auto post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void;
		auto post_non_nestable(task_type task) -> void;
		virtual auto post_delayed(task_type task, steady_clock::time_point timeout) -> void = 0;
		virtual auto post(task_type task) -> void = 0;
};

// The result of `IdleTasksEnabled` (& co.) will change depending on whether or not `Self`
// implements `post_idle` (& co.)
template <class Self>
class task_runner::task_runner_of : public task_runner {
	private:
		friend Self;
		task_runner_of() = default;

	public:
		[[nodiscard]] auto IdleTasksEnabled() -> bool final;
		[[nodiscard]] auto NonNestableTasksEnabled() const -> bool final;
		[[nodiscard]] auto NonNestableDelayedTasksEnabled() const -> bool final;

	protected:
		auto PostTaskImpl(task_type task, const v8::SourceLocation& location) -> void final;
		auto PostNonNestableTaskImpl(task_type task, const v8::SourceLocation& location) -> void final;
		auto PostDelayedTaskImpl(task_type task, double delay_in_seconds, const v8::SourceLocation& location) -> void final;
		auto PostNonNestableDelayedTaskImpl(task_type task, double delay_in_seconds, const v8::SourceLocation& location) -> void final;
		auto PostIdleTaskImpl(idle_task task, const v8::SourceLocation& location) -> void final;

	private:
		auto self() -> Self& { return *static_cast<Self*>(this); }
};

// ---

// task_runner
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto task_runner::post_idle(idle_task /*task*/) -> void {
	std::unreachable();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto task_runner::post_non_nestable_delayed(task_type /*task*/, steady_clock::time_point /*timeout*/) -> void {
	std::unreachable();
};

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto task_runner::post_non_nestable(task_type /*task*/) -> void {
	std::unreachable();
}

// task_runner::task_runner_of<T>
template <class Self>
auto task_runner::task_runner_of<Self>::IdleTasksEnabled() -> bool {
	return &Self::post_idle != &task_runner::post_idle;
}

template <class Self>
auto task_runner::task_runner_of<Self>::NonNestableTasksEnabled() const -> bool {
	return &Self::post_non_nestable != &task_runner::post_non_nestable;
}

template <class Self>
auto task_runner::task_runner_of<Self>::NonNestableDelayedTasksEnabled() const -> bool {
	return &Self::post_non_nestable_delayed != &task_runner::post_non_nestable_delayed;
}

template <class Self>
auto task_runner::task_runner_of<Self>::PostTaskImpl(task_type task, const v8::SourceLocation& /*location*/
) -> void {
	return self().post(std::move(task));
}

template <class Self>
auto task_runner::task_runner_of<Self>::PostNonNestableTaskImpl(task_type task, const v8::SourceLocation& /*location*/
) -> void {
	return self().post_non_nestable(std::move(task));
}

template <class Self>
auto task_runner::task_runner_of<Self>::PostDelayedTaskImpl(task_type task, double delay_in_seconds, const v8::SourceLocation& /*location*/
) -> void {
	auto timeout = duration_cast<steady_clock::duration>(std::chrono::duration<double>{delay_in_seconds});
	return self().post_delayed(std::move(task), steady_clock::now() + timeout);
}

template <class Self>
auto task_runner::task_runner_of<Self>::PostNonNestableDelayedTaskImpl(task_type task, double delay_in_seconds, const v8::SourceLocation& /*location*/
) -> void {
	auto timeout = duration_cast<steady_clock::duration>(std::chrono::duration<double>{delay_in_seconds});
	return self().post_non_nestable_delayed(std::move(task), steady_clock::now() + timeout);
}

template <class Self>
auto task_runner::task_runner_of<Self>::PostIdleTaskImpl(idle_task task, const v8::SourceLocation& /*location*/
) -> void {
	return self().post_idle(std::move(task));
}

} // namespace isolated_v8
