module;
#include <chrono>
#include <optional>
module v8_js;
import auto_js;
using namespace std::chrono;

namespace js::iv8::isolated::clock {

// deterministic
deterministic::deterministic(
	js_clock::time_point epoch,
	js_clock::duration increment
) :
		time_point_{clock_cast<system_clock>(epoch)},
		increment_{duration_cast<system_clock::duration>(increment)} {}

auto deterministic::clock_time() -> system_clock::time_point {
	auto result = time_point_;
	time_point_ += increment_;
	return result;
}

// microtask
microtask::microtask(std::optional<js_clock::time_point> maybe_epoch) :
		microtask{
			maybe_epoch
				? microtask{clock_cast<system_clock>(*maybe_epoch)}
				: microtask{system_clock::now()}
		} {}

microtask::microtask(system_clock::time_point epoch) :
		offset_{duration_cast<system_clock::duration>(steady_clock::now().time_since_epoch()) - epoch.time_since_epoch()} {}

auto microtask::begin_tick() -> void {
	time_point_ = steady_clock::now();
}

auto microtask::clock_time() -> system_clock::time_point {
	return system_clock::time_point{duration_cast<system_clock::duration>(time_point_.time_since_epoch())} - offset_;
}

// realtime
realtime::realtime(js_clock::time_point epoch) :
		offset_{(steady_clock::now() - duration_cast<steady_clock::duration>(epoch.time_since_epoch())).time_since_epoch()} {}

auto realtime::clock_time() -> system_clock::time_point {
	return system_clock::time_point{duration_cast<system_clock::duration>((steady_clock::now() - offset_).time_since_epoch())};
}

// system
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto system::clock_time() -> system_clock::time_point {
	return system_clock::now();
}

} // namespace js::iv8::isolated::clock
