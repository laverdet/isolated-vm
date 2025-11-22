module;
#include <chrono>
export module auto_js:intrinsics.date;

namespace js {

// `Clock` implementation for JavaScript time. The point is to avoid casting from `double` to
// `int64_t` where possible and still be able to use `clock_cast` when needed. The epochs are the
// same so not much work is needed.
export class js_clock {
	public:
		using rep = double;
		using period = std::milli;
		using duration = std::chrono::duration<rep, period>;
		using time_point = std::chrono::time_point<js_clock>;
		constexpr static auto is_steady = false;

		static auto now() noexcept -> time_point;
};

auto js_clock::now() noexcept -> time_point {
	// C++20: system_clock measures Unix Time (i.e., time since 00:00:00 Coordinated Universal Time
	// (UTC), Thursday, 1 January 1970, not counting leap seconds).
	auto time_ms = duration_cast<js_clock::duration>(std::chrono::system_clock::now().time_since_epoch());
	return time_point{time_ms};
};

} // namespace js

namespace std::chrono {
using namespace js;

// libc++ polyfill
// https://github.com/llvm/llvm-project/issues/166050
#ifdef _LIBCPP_VERSION
export template <class To, class From>
struct clock_time_conversion;

export template <class To, class From, class Duration>
auto clock_cast(std::chrono::time_point<From, Duration> time_point) {
	clock_time_conversion<To, From> convert{};
	return convert(time_point);
}
#endif

// ---

template <>
struct clock_time_conversion<system_clock, js_clock> {
		constexpr auto operator()(js_clock::time_point time) -> system_clock::time_point {
			return system_clock::time_point{duration_cast<system_clock::duration>(time.time_since_epoch())};
		}
};

template <>
struct clock_time_conversion<js_clock, system_clock> {
		constexpr auto operator()(system_clock::time_point time) -> js_clock::time_point {
			return js_clock::time_point{duration_cast<js_clock::duration>(time.time_since_epoch())};
		}
};

} // namespace std::chrono
