module;
#include <chrono>
#include <cstring>
#include <memory>
#include <variant>
module v8_js;
import :isolated.agent;
import :isolated.platform;

namespace js::iv8::isolated {

auto isolated_agent_delegate::GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return agent_host::get_current(isolate).task_runner(priority);
}

auto isolated_agent_delegate::CurrentClockTimeMilliseconds() -> int64_t {
	return std::visit(
		[](auto& clock) -> int64_t {
			auto unix_time = std::chrono::system_clock::time_point{clock.clock_time()};
			auto ms_time = duration_cast<milliseconds>(unix_time.time_since_epoch());
			return static_cast<int64_t>(ms_time.count());
		},
		agent_host::get_current(v8::Isolate::GetCurrent()).clock()
	);
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

auto isolated_platform::acquire() -> platform::platform_handle {
	return platform::platform_handle::acquire<isolated_platform>();
}

} // namespace js::iv8::isolated
