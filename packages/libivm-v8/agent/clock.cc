module;
#include <chrono>
#include <optional>
module ivm.isolated_v8;
import :agent;
import ivm.utility;
import ivm.value;

using namespace std::chrono;
using ivm::value::js_clock;

namespace ivm {

// base_clock
auto agent::clock::base_clock::begin_tick() -> void {}

auto agent::clock::base_clock::steady_clock_offset(js_clock::time_point epoch) -> steady_clock::duration {
	return duration_cast<steady_clock::duration>(epoch.time_since_epoch()) - steady_clock::now().time_since_epoch();
}

// deterministic
agent::clock::deterministic::deterministic(
	js_clock::time_point epoch,
	js_clock::duration increment
) :
		epoch_{clock_cast<utc_clock>(epoch)},
		increment_{duration_cast<utc_clock::duration>(increment)} {}

auto agent::clock::deterministic::clock_time() -> utc_clock::time_point {
	auto result = epoch_;
	epoch_ += increment_;
	return result;
}

// microtask
agent::clock::microtask::microtask(std::optional<js_clock::time_point> maybe_epoch) :
		offset_{steady_clock_offset(*maybe_epoch.or_else([]() { return std::optional{js_clock::now()}; }))},
		time_point_{steady_clock::now() + offset_} {}

auto agent::clock::microtask::begin_tick() -> void {
	time_point_ = steady_clock::now() + offset_;
}

auto agent::clock::microtask::clock_time() -> steady_clock::time_point {
	return time_point_;
}

// realtime
agent::clock::realtime::realtime(js_clock::time_point epoch) :
		offset_{steady_clock_offset(epoch)} {}

auto agent::clock::realtime::clock_time() -> steady_clock::time_point {
	return steady_clock::now() + offset_;
}

// system
auto agent::clock::system::clock_time() -> system_clock::time_point {
	return system_clock::now();
}

} // namespace ivm
