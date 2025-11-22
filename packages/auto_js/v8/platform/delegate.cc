module;
#include <cassert>
#include <chrono>
#include <memory>
#include <random>
#include <ranges>
#include <typeinfo>
module v8_js;
import :platform.delegate;

namespace js::iv8::platform {

// `platform_handle`
struct platform_instance_handle {
		std::weak_ptr<v8::Platform> platform;
		const std::type_info* type;
};

util::lockable<platform_instance_handle> shared_platform;

platform_handle::platform_handle(const std::type_info& type, make_type* make) :
		platform_{util::elide{[ & ]() -> std::shared_ptr<v8::Platform> {
			auto lock = shared_platform.write();
			auto acquired_platform = lock->platform.lock();
			if (acquired_platform) {
				assert(*lock->type == type);
			} else {
				acquired_platform = make();
				lock->platform = acquired_platform;
				lock->type = &type;
			}
			return acquired_platform;
		}}} {}

platform_handle::~platform_handle() {
	// nb: Platform must be released with the lock, in case this is the last handle. That way if one
	// thread is releasing while another is trying to acquire then v8 will fully shutdown and then
	// start back up again.
	if (platform_) {
		auto lock = shared_platform.write();
		platform_.reset();
	}
}

// `initialization_delegate`
initialization_delegate::initialization_delegate(v8::Platform* implementation) {
	v8::V8::InitializeICU();
	v8::V8::InitializePlatform(implementation);
	v8::V8::Initialize();
}

initialization_delegate::~initialization_delegate() {
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
}

// `background_worker_delegate`
auto background_worker_delegate::NumberOfWorkerThreads() -> int {
	return static_cast<int>(worker_runner_.thread_count());
};

auto background_worker_delegate::CreateJobImpl(v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task, const v8::SourceLocation& /*location*/) -> std::unique_ptr<v8::JobHandle> {
	return v8::platform::NewDefaultJobHandle(this, priority, std::move(job_task), NumberOfWorkerThreads());
};

auto background_worker_delegate::PostTaskOnWorkerThreadImpl(v8::TaskPriority priority, std::unique_ptr<v8::Task> task, const v8::SourceLocation& /*location*/) -> void {
	worker_runner_.post(priority, std::move(task));
};

auto background_worker_delegate::PostDelayedTaskOnWorkerThreadImpl(v8::TaskPriority /*priority*/, std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& /*location*/) -> void {
	auto duration = duration_cast<steady_clock::duration>(std::chrono::duration<double>{delay_in_seconds});
	worker_runner_.post_delayed(std::move(task), duration);
};

// `entropy_delegate_initializer`
entropy_delegate_initializer::entropy_delegate_initializer(v8::EntropySource source) {
	v8::V8::SetEntropySource(source);
}

entropy_delegate_initializer::~entropy_delegate_initializer() {
	v8::V8::SetEntropySource(nullptr);
}

auto entropy_delegate_initializer::fill_random_bytes(unsigned char* buffer, size_t length) -> bool {
	std::random_device device;
	auto byte_view =
		std::views::repeat(std::nullopt) |
		std::views::transform([ & ](auto /*nullopt*/) { return device(); }) |
		std::views::transform([](auto value) {
			return std::bit_cast<std::array<unsigned char, sizeof(value)>>(value);
		}) |
		std::views::join;
	std::ranges::copy_n(byte_view.begin(), static_cast<std::ptrdiff_t>(length), buffer);
	return true;
}

// `instrumentation_delegate`
auto instrumentation_delegate::MonotonicallyIncreasingTime() -> double {
	// I think this is only used for internal instrumentation, so no need to delegate to the agent
	auto duration = steady_clock::now().time_since_epoch();
	return static_cast<double>(duration_cast<seconds>(duration).count());
}

auto instrumentation_delegate::GetTracingController() -> v8::TracingController* {
	return &tracing_controller_;
}

} // namespace js::iv8::platform
