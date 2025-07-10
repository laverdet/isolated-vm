module;
#include <chrono>
#include <optional>
#include <variant>
export module isolated_v8:clock;
import isolated_js;

using namespace std::chrono;
using js::js_clock;
#ifdef _LIBCPP_VERSION
namespace std::chrono {
using utc_clock = system_clock;
}
#endif

namespace isolated_v8::clock {

class base_clock {
	public:
		auto begin_tick() -> void;
		auto clock_time_ms(this auto& self) -> int64_t;

	protected:
		static auto steady_clock_offset(js_clock::time_point epoch) -> steady_clock::duration;
};

auto base_clock::clock_time_ms(this auto& self) -> int64_t {
	auto some_time_point = self.clock_time();
	// You can't `clock_cast` from `steady_clock` but internally we redefine that epoch to be UTC via
	// `steady_clock_offset`, so casting the `duration` directly is correct for clocks that use that
	// reference.
	auto utc_duration = duration_cast<utc_clock::duration>(some_time_point.time_since_epoch());
	return static_cast<int64_t>(duration_cast<milliseconds>(utc_duration).count());
}

// Starts at a specified epoch and each query for the time will increment the clock by a constant
// amount.
export class deterministic : public base_clock {
	public:
		deterministic(js_clock::time_point epoch, js_clock::duration increment);

		auto clock_time() -> utc_clock::time_point;

	private:
		utc_clock::time_point epoch_;
		utc_clock::duration increment_;
};

// During a microtask the clock will stay the same. It will update to the current time at the start
// of the next tick. Optionally you can specify a start epoch.
export class microtask : public base_clock {
	public:
		explicit microtask(std::optional<js_clock::time_point> maybe_epoch);

		auto begin_tick() -> void;
		auto clock_time() -> steady_clock::time_point;

	private:
		steady_clock::duration offset_;
		steady_clock::time_point time_point_;
};

// Realtime clock starting from the given epoch.
export class realtime : public base_clock {
	public:
		explicit realtime(js_clock::time_point epoch);

		auto clock_time() -> steady_clock::time_point;

	private:
		steady_clock::duration offset_;
};

// Pass through system time
export class system : public base_clock {
	public:
		system() = default;
		auto clock_time() -> system_clock::time_point;
		auto foo() -> void;
};

export using any_clock = std::variant<deterministic, microtask, realtime, system>;

} // namespace isolated_v8::clock

;
