module;
#include <memory>
#include <thread>
export module ivm.isolated_v8:platform.task_runner;
import v8;

namespace ivm {

export class task_runner : public v8::TaskRunner {
	public:
		virtual bool IdleTasksEnabled() { return false; }
		virtual bool NonNestableTasksEnabled() const { return true; }
		virtual bool NonNestableDelayedTasksEnabled() const { return false; }

		virtual ~task_runner() = default;

	protected:
		auto PostTaskImpl(std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void final {
			task->Run();
		}
		auto PostNonNestableTaskImpl(std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void final {
		}
		auto PostDelayedTaskImpl(std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void final {
			task->Run();
		}
		auto PostNonNestableDelayedTaskImpl(std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void final {
		}
		auto PostIdleTaskImpl(std::unique_ptr<v8::IdleTask> task, const v8::SourceLocation& location) -> void final {
			task->Run(0);
		}
};

} // namespace ivm
