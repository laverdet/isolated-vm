module;
#include <memory>
export module isolated_v8:isolated_platform;
import v8_js;
import v8;

namespace isolated_v8 {

class isolated_agent_delegate
		: virtual public v8::Platform,
			public js::iv8::platform::background_worker_delegate,
			public js::iv8::platform::instrumentation_delegate,
			public js::iv8::platform::platform_entropy_delegate<isolated_agent_delegate> {
	public:
		auto CurrentClockTimeMilliseconds() -> int64_t override;
		auto CurrentClockTimeMillis() -> double override;
		auto GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> override;

		static auto fill_random_bytes_for_isolate(v8::Isolate* isolate, unsigned char* buffer, size_t length) -> bool;
};

class isolated_platform final
		: public isolated_agent_delegate,
			public js::iv8::platform::initialization_delegate {
	public:
		isolated_platform() : initialization_delegate{this} {}
		static auto acquire() -> js::iv8::platform::platform_handle;
};

} // namespace isolated_v8
