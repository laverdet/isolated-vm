module;
#include <chrono>
#include <optional>
module ivm.isolated_v8;
import ivm.utility;
import ivm.value;

using namespace std::chrono;
using ivm::js::js_clock;
#ifdef _LIBCPP_VERSION
namespace std::chrono {
using utc_clock = system_clock;
}
#endif

namespace ivm::clock {

// base_clock
auto base_clock::begin_tick() -> void {}

auto base_clock::steady_clock_offset(js_clock::time_point epoch) -> steady_clock::duration {
	return duration_cast<steady_clock::duration>(epoch.time_since_epoch()) - steady_clock::now().time_since_epoch();
}

// deterministic
deterministic::deterministic(
	js_clock::time_point epoch,
	js_clock::duration increment
) :
		epoch_{duration_cast<utc_clock::duration>(epoch.time_since_epoch())},
		increment_{duration_cast<utc_clock::duration>(increment)} {}

auto deterministic::clock_time() -> utc_clock::time_point {
	auto result = epoch_;
	epoch_ += increment_;
	return result;
}

// microtask
microtask::microtask(std::optional<js_clock::time_point> maybe_epoch) :
		offset_{steady_clock_offset(*maybe_epoch.or_else([]() { return std::optional{js_clock::now()}; }))},
		time_point_{steady_clock::now() + offset_} {}

auto microtask::begin_tick() -> void {
	time_point_ = steady_clock::now() + offset_;
}

auto microtask::clock_time() -> steady_clock::time_point {
	return time_point_;
}

// realtime
realtime::realtime(js_clock::time_point epoch) :
		offset_{steady_clock_offset(epoch)} {}

auto realtime::clock_time() -> steady_clock::time_point {
	return steady_clock::now() + offset_;
}

// system
auto system::clock_time() -> system_clock::time_point {
	return system_clock::now();
}

} // namespace ivm::clock
