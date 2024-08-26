module;
#include <memory>
#include <thread>
module ivm.isolated_v8;
import :platform;
import v8;

namespace ivm {

auto task_runner::IdleTasksEnabled() -> bool {
	return false;
}

auto task_runner::NonNestableTasksEnabled() const -> bool {
	return true;
}

auto task_runner::NonNestableDelayedTasksEnabled() const -> bool {
	return false;
}

auto task_runner::PostTaskImpl(std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void {
	task->Run();
}

auto task_runner::PostNonNestableTaskImpl(std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void {
}

auto task_runner::PostDelayedTaskImpl(std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void {
	task->Run();
}

auto task_runner::PostNonNestableDelayedTaskImpl(std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void {
}

auto task_runner::PostIdleTaskImpl(std::unique_ptr<v8::IdleTask> task, const v8::SourceLocation& location) -> void {
	task->Run(0);
}

} // namespace ivm
