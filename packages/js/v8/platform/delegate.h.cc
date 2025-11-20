module;
#include <cassert>
#include <cstddef>
#include <memory>
#include <typeinfo>
export module v8_js:platform.delegate;
import :platform.worker_runner;
import ivm.utility;
import v8;

namespace js::iv8::platform {

// Ensures that exactly one platform is registered at a time, since it is shared for the whole
// process. This should be held by each cluster (maybe many per process).
export class platform_handle {
	public:
		template <class Delegate>
		static auto acquire() -> platform_handle;

		~platform_handle();
		platform_handle(platform_handle&&) = default;
		platform_handle(const platform_handle&) = default;
		auto operator=(platform_handle&&) -> platform_handle& = default;
		auto operator=(const platform_handle&) -> platform_handle& = default;

	private:
		using make_type = auto() -> std::shared_ptr<v8::Platform>;
		platform_handle(const std::type_info& type, make_type* make);

		std::shared_ptr<v8::Platform> platform_;
};

// Register platform delegate with v8
export class initialization_delegate : util::non_copyable {
	public:
		explicit initialization_delegate(v8::Platform* implementation);
		~initialization_delegate();
};

// Handles background jobs / tasks
export class background_worker_delegate : virtual public v8::Platform {
	public:
		auto NumberOfWorkerThreads() -> int override;

	protected:
		auto CreateJobImpl(v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task, const v8::SourceLocation& location) -> std::unique_ptr<v8::JobHandle> override;
		auto PostTaskOnWorkerThreadImpl(v8::TaskPriority priority, std::unique_ptr<v8::Task> task, const v8::SourceLocation& location) -> void override;
		auto PostDelayedTaskOnWorkerThreadImpl(v8::TaskPriority priority, std::unique_ptr<v8::Task> task, double delay_in_seconds, const v8::SourceLocation& location) -> void override;

	private:
		worker_runner worker_runner_;
};

// Entropy source. This is used for both the page allocator, context RNG seed, maybe others.
class entropy_delegate_initializer : util::non_copyable {
	public:
		explicit entropy_delegate_initializer(v8::EntropySource source);
		~entropy_delegate_initializer();

		static auto fill_random_bytes(unsigned char* buffer, size_t length) -> bool;
};

export template <class Delegate>
class platform_entropy_delegate : public entropy_delegate_initializer {
	public:
		platform_entropy_delegate() : entropy_delegate_initializer{entropy_source} {}

	private:
		static auto entropy_source(unsigned char* buffer, size_t length) -> bool;
};

// Instrumentation and tracing
export class instrumentation_delegate : virtual public v8::Platform {
	public:
		auto GetTracingController() -> v8::TracingController* override;
		auto MonotonicallyIncreasingTime() -> double override;

		// There isn't really a great place to put this right now
		auto GetPageAllocator() -> v8::PageAllocator* override { return nullptr; }

	private:
		v8::TracingController tracing_controller_;
};

// ---

template <class Delegate>
auto platform_handle::acquire() -> platform_handle {
	constexpr auto make = []() -> std::shared_ptr<v8::Platform> {
		return std::make_shared<Delegate>();
	};
	return platform_handle{typeid(Delegate), make};
}

template <class Delegate>
auto platform_entropy_delegate<Delegate>::entropy_source(unsigned char* buffer, size_t length) -> bool {
	if (auto* isolate = v8::Isolate::TryGetCurrent()) {
		if (Delegate::fill_random_bytes_for_isolate(isolate, buffer, length)) {
			return true;
		}
	}
	return fill_random_bytes(buffer, length);
}

} // namespace js::iv8::platform
