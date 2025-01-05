module;
#include <memory>
#include <thread>
export module ivm.isolated_v8:platform;
import v8;
import ivm.utility;

// See: /v8/src/libplatform/default-job.cc

namespace ivm {

export class job_handle : public v8::JobHandle, public v8::JobDelegate {
	public:
		explicit job_handle(std::unique_ptr<v8::JobTask> job_task);
		~job_handle() override = default;

		// JobHandle
		auto Cancel() -> void override;
		auto CancelAndDetach() -> void override;
		auto IsActive() -> bool override;
		auto IsValid() -> bool final;
		auto Join() -> void override;
		auto NotifyConcurrencyIncrease() -> void override;
		auto UpdatePriority(v8::TaskPriority new_priority) -> void final;
		[[nodiscard]] auto UpdatePriorityEnabled() const -> bool final;

		// JobDelegate
		auto GetTaskId() -> uint8_t override;
		[[nodiscard]] auto IsJoiningThread() const -> bool override;
		auto ShouldYield() -> bool override;

	private:
		std::unique_ptr<v8::JobTask> job_task;
		std::thread thread;
};

// Once per process, performs initialization of v8. Process-wide shared state is managed in this
// class.
export class platform : util::non_moveable, public v8::Platform {
	public:
		class handle;

		platform();
		~platform() override;

		auto GetTracingController() -> v8::TracingController* final;
		auto GetPageAllocator() -> v8::PageAllocator* final;
		auto NumberOfWorkerThreads() -> int final;

		auto MonotonicallyIncreasingTime() -> double final;
		auto CurrentClockTimeMilliseconds() -> int64_t final;
		auto CurrentClockTimeMillis() -> double final;

		auto GetForegroundTaskRunner(
			v8::Isolate* isolate
		) -> std::shared_ptr<v8::TaskRunner> final;
		auto GetForegroundTaskRunner(
			v8::Isolate* isolate,
			v8::TaskPriority priority
		) -> std::shared_ptr<v8::TaskRunner> final;

	protected:
		auto CreateJobImpl(
			v8::TaskPriority priority,
			std::unique_ptr<v8::JobTask> job_task,
			const v8::SourceLocation& location
		) -> std::unique_ptr<v8::JobHandle> final;
		auto PostTaskOnWorkerThreadImpl(
			v8::TaskPriority priority,
			std::unique_ptr<v8::Task> task,
			const v8::SourceLocation& location
		) -> void final;
		auto PostDelayedTaskOnWorkerThreadImpl(
			v8::TaskPriority priority,
			std::unique_ptr<v8::Task> task,
			double delay_in_seconds,
			const v8::SourceLocation& location
		) -> void final;

	private:
		static auto fill_random_bytes(unsigned char* buffer, size_t length) -> bool;

		std::unique_ptr<v8::Platform> default_platform_{v8::platform::NewDefaultPlatform()};
};

// Responsible for initializing v8 and creating one `platform` per process. When the last handle is
// destroyed, the `platform` is destroyed and v8 is shut down.
class platform::handle {
	public:
		handle(const handle&) = default;
		handle(handle&&) = default;
		auto operator=(const handle&) -> handle& = default;
		auto operator=(handle&&) noexcept -> handle& = default;
		~handle();

		static auto acquire() -> handle;

	private:
		explicit handle(std::shared_ptr<platform> platform);
		std::shared_ptr<platform> platform_;
};

} // namespace ivm
