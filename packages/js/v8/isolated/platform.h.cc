module;
#include <memory>
export module v8_js:isolated.platform;
import :platform.delegate;
import v8;

namespace js::iv8::isolated {

class isolated_agent_delegate
		: virtual public v8::Platform,
			public platform::background_worker_delegate,
			public platform::instrumentation_delegate,
			public platform::platform_entropy_delegate<isolated_agent_delegate> {
	public:
		auto CurrentClockTimeMilliseconds() -> int64_t override;
		auto CurrentClockTimeMillis() -> double override;
		auto GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> override;

		static auto fill_random_bytes_for_isolate(v8::Isolate* isolate, unsigned char* buffer, size_t length) -> bool;
};

class isolated_platform final
		: public isolated_agent_delegate,
			public platform::initialization_delegate {
	public:
		isolated_platform() : initialization_delegate{this} {}
		static auto acquire() -> platform::platform_handle;
};

} // namespace js::iv8::isolated
