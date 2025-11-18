module;
#include <cstring>
#include <memory>
module isolated_v8;
import :agent_host;
import :isolated_platform;

namespace isolated_v8 {

auto isolated_agent_delegate::GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return agent_host::get_current(isolate).task_runner(priority);
}

auto isolated_agent_delegate::CurrentClockTimeMilliseconds() -> int64_t {
	return agent_host::get_current()->clock_time_ms();
}

auto isolated_agent_delegate::CurrentClockTimeMillis() -> double {
	return static_cast<double>(CurrentClockTimeMilliseconds());
}

auto isolated_agent_delegate::fill_random_bytes_for_isolate(v8::Isolate* isolate, unsigned char* buffer, size_t length) -> bool {
	if (length == sizeof(int64_t)) {
		auto& host = agent_host::get_current(isolate);
		auto seed = host.take_random_seed();
		if (seed) {
			std::memcpy(buffer, &seed.value(), sizeof(int64_t));
			return true;
		}
	}
	return fill_random_bytes(buffer, length);
}

auto isolated_platform::acquire() -> js::iv8::platform::platform_handle {
	return js::iv8::platform::platform_handle::acquire<isolated_platform>();
}

} // namespace isolated_v8
