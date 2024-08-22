module;
#include <chrono>
#include <functional>
#include <optional>
#include <variant>
export module ivm.isolated_v8:agent.clock;
import :agent.fwd;
import ivm.utility;
import ivm.value;

using namespace std::chrono;
using ivm::value::js_clock;

namespace ivm {

struct agent::clock {
		template <class Self>
		class base_clock;
		class deterministic;
		class microtask;
		class realtime;
		class system;
		using any_clock = std::variant<deterministic, microtask, realtime, system>;
};

template <class Self>
class agent::clock::base_clock {
	public:
		auto begin_tick() -> void {}

		auto clock_time_ms() -> int64_t {
			auto some_time_point = static_cast<Self*>(this)->clock_time();
			// You can't `clock_cast` from `steady_clock` but internally we redefine that epoch to be UTC
			// via `steady_clock_offset`, so casting the `duration` directly is correct for clocks that
			// use that reference.
			auto utc_duration = duration_cast<utc_clock::duration>(some_time_point.time_since_epoch());
			return static_cast<int64_t>(duration_cast<milliseconds>(utc_duration).count());
		}

	protected:
		auto steady_clock_offset(js_clock::time_point epoch) -> steady_clock::duration {
			return duration_cast<steady_clock::duration>(epoch.time_since_epoch()) - steady_clock::now().time_since_epoch();
		}
};

// Starts at a specified epoch and each query for the time will increment the clock by a constant
// amount.
class agent::clock::deterministic : public base_clock<deterministic> {
	public:
		deterministic(
			js_clock::time_point epoch,
			js_clock::duration increment
		) :
				epoch_{clock_cast<utc_clock>(epoch)},
				increment_{duration_cast<utc_clock::duration>(increment)} {}

		auto clock_time() -> utc_clock::time_point {
			auto result = epoch_;
			epoch_ += increment_;
			return result;
		}

	private:
		utc_clock::time_point epoch_;
		utc_clock::duration increment_;
};

// During a microtask the clock will stay the same. It will update to the current time at the start
// of the next tick. Optionally you can specify a start epoch.
class agent::clock::microtask : public base_clock<microtask> {
	public:
		microtask(std::optional<js_clock::time_point> maybe_epoch) :
				offset_{steady_clock_offset(*maybe_epoch.or_else([]() { return std::optional{js_clock::now()}; }))},
				time_point_{steady_clock::now() + offset_} {}

		auto begin_tick() -> void {
			time_point_ = steady_clock::now() + offset_;
		}

		auto clock_time() -> steady_clock::time_point {
			return time_point_;
		}

	private:
		steady_clock::duration offset_;
		steady_clock::time_point time_point_;
};

// Realtime clock starting from the given epoch.
class agent::clock::realtime : public base_clock<realtime> {
	public:
		realtime(js_clock::time_point epoch) :
				offset_{steady_clock_offset(epoch)} {}

		auto clock_time() -> steady_clock::time_point {
			return steady_clock::now() + offset_;
		}

	private:
		steady_clock::duration offset_;
};

// Pass through system time
class agent::clock::system : public base_clock<system> {
	public:
		auto clock_time() -> system_clock::time_point {
			return system_clock::now();
		}
};

} // namespace ivm
