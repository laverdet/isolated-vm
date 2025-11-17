module;
#include <chrono>
#include <optional>
#include <variant>
export module v8_js:isolated.clock;
import isolated_js;
using namespace std::chrono;

namespace js::iv8::isolated::clock {

// Starts at a specified epoch and each query for the time will increment the clock by a constant
// amount.
export class deterministic {
	public:
		deterministic(js_clock::time_point epoch, js_clock::duration increment);

		auto begin_tick() -> void {}
		auto clock_time() -> system_clock::time_point;

	private:
		system_clock::time_point time_point_;
		system_clock::duration increment_;
};

// During a microtask the clock will stay the same. It will update to the current time at the start
// of the next tick. Optionally you can specify a start epoch.
export class microtask {
	public:
		explicit microtask(std::optional<js_clock::time_point> maybe_epoch);

		auto begin_tick() -> void;
		auto clock_time() -> system_clock::time_point;

	private:
		explicit microtask(system_clock::time_point epoch);

		steady_clock::time_point time_point_;
		system_clock::duration offset_;
};

// Realtime clock starting from the given epoch.
export class realtime {
	public:
		explicit realtime(js_clock::time_point epoch);

		auto begin_tick() -> void {}
		auto clock_time() -> system_clock::time_point;

	private:
		steady_clock::duration offset_;
};

// Pass through system time
export class system {
	public:
		auto begin_tick() -> void {}
		auto clock_time() -> system_clock::time_point;
};

export using any_clock = std::variant<deterministic, microtask, realtime, system>;

} // namespace js::iv8::isolated::clock
