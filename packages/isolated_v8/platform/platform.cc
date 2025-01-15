module;
#include <chrono>
#include <cstring>
#include <memory>
#include <random>
#include <ranges>
module isolated_v8.platform;
import isolated_v8.agent;
import ivm.utility;
import v8;

namespace isolated_v8 {

platform::platform() {
	v8::V8::InitializeICU();
	v8::V8::InitializePlatform(this);
	v8::V8::SetEntropySource(fill_random_bytes);
	v8::V8::Initialize();
}

platform::~platform() {
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
}

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

auto platform::GetTracingController() -> v8::TracingController* {
	return default_platform_->GetTracingController();
}

auto platform::GetPageAllocator() -> v8::PageAllocator* {
	return nullptr;
}

auto platform::NumberOfWorkerThreads() -> int {
	return 0;
}

auto platform::GetForegroundTaskRunner(v8::Isolate* isolate) -> std::shared_ptr<v8::TaskRunner> {
	return GetForegroundTaskRunner(isolate, v8::TaskPriority::kUserBlocking);
}

auto platform::GetForegroundTaskRunner(
	v8::Isolate* isolate,
	v8::TaskPriority priority
) -> std::shared_ptr<v8::TaskRunner> {
	return agent::host::get_current(isolate).task_runner(priority);
}

auto platform::CreateJobImpl(
	v8::TaskPriority priority,
	std::unique_ptr<v8::JobTask> job_task,
	const v8::SourceLocation& location
) -> std::unique_ptr<v8::JobHandle> {
	return default_platform_->CreateJob(priority, std::move(job_task), location);
};

auto platform::PostTaskOnWorkerThreadImpl(
	v8::TaskPriority /*priority*/,
	std::unique_ptr<v8::Task> task,
	const v8::SourceLocation& location
) -> void {
	default_platform_->CallOnWorkerThread(std::move(task), location);
};

auto platform::PostDelayedTaskOnWorkerThreadImpl(
	v8::TaskPriority /*priority*/,
	std::unique_ptr<v8::Task> task,
	double delay_in_seconds,
	const v8::SourceLocation& location
) -> void {
	default_platform_->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds, location);
};

auto platform::fill_random_bytes(unsigned char* buffer, size_t length) -> bool {
	// Copies the bit data from an infinite view of numerics into `buffer` up to `length`.
	auto fill = [ & ](auto&& numeric_stream) {
		auto bytes_views =
			numeric_stream |
			std::views::transform([](auto value) {
				return std::bit_cast<std::array<unsigned char, sizeof(value)>>(value);
			});
		auto byte_view = std::views::join(bytes_views);
		std::ranges::copy_n(byte_view.begin(), length, buffer);
		return true;
	};
	auto* host = agent::host::get_current();
	if (host != nullptr) {
		auto seed = host->take_random_seed();
		if (seed) {
			// Repeats the user-supplied double seed
			return fill(std::ranges::repeat_view(*seed));
		}
	}
	// Stream pulls directly from `random_device`
	std::random_device device;
	auto integer_stream =
		std::views::repeat(std::nullopt) |
		std::views::transform([ & ](auto /*nullopt*/) { return device(); });
	return fill(integer_stream);
}

} // namespace isolated_v8
