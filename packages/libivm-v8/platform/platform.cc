module;
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
export module ivm.isolated_v8:platform;
import :agent;
import :platform.job_handle;
import :platform.task_runner;
import ivm.utility;
import v8;

namespace ivm {

// Once per process, performs initialization of v8. Process-wide shared state is managed in this
// class.
export class platform : non_moveable, public v8::Platform {
	public:
		platform();
		virtual ~platform();

		auto GetTracingController() -> v8::TracingController* final {
			return default_platform_->GetTracingController();
		}
		auto GetPageAllocator() -> v8::PageAllocator* final { return nullptr; }
		auto NumberOfWorkerThreads() -> int final { return 0; }

		// Clock
		auto MonotonicallyIncreasingTime() -> double final;
		auto CurrentClockTimeMilliseconds() -> int64_t final;
		auto CurrentClockTimeMillis() -> double final;
		auto GetForegroundTaskRunner(v8::Isolate* isolate) -> std::shared_ptr<v8::TaskRunner> final {
			return default_platform_->GetForegroundTaskRunner(isolate);
		}
		auto GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> final {
			return default_platform_->GetForegroundTaskRunner(isolate, priority);
		}

	protected:
		auto CreateJobImpl(v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task, const v8::SourceLocation& location) -> std::unique_ptr<v8::JobHandle> final {
			return default_platform_->CreateJob(priority, std::move(job_task), location);
		};

		auto PostTaskOnWorkerThreadImpl(v8::TaskPriority priority, std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void final {
			default_platform_->CallOnWorkerThread(std::move(task), location);
		};

		auto PostDelayedTaskOnWorkerThreadImpl(v8::TaskPriority priority, std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void final {
			default_platform_->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds, location);
		};

	private:
		static bool fill_random_bytes(unsigned char* buffer, size_t length);

		std::unique_ptr<v8::Platform> default_platform_{v8::platform::NewDefaultPlatform()};
		// std::shared_ptr<v8::TaskRunner> task_runner_;
		// std::unique_ptr<v8::TracingController> tracing_controller;
};

platform::~platform() {
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
}

// auto platform::GetTracingController() -> v8::TracingController* {
// 	return tracing_controller.get();
// }

auto platform::MonotonicallyIncreasingTime() -> double {
	// I think this is only used for internal instrumentation, so no need to delegate to the agent
	using namespace std::chrono;
	auto duration = steady_clock::now().time_since_epoch();
	return static_cast<double>(duration_cast<seconds>(duration).count());
}

auto platform::CurrentClockTimeMilliseconds() -> int64_t {
	return agent::host::get_current()->clock_time_ms();
}

auto platform::CurrentClockTimeMillis() -> double {
	return static_cast<double>(CurrentClockTimeMilliseconds());
}

} // namespace ivm
