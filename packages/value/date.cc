module;
#include <chrono>
export module ivm.value:date;
import :tag;

namespace ivm::value {

// `Clock` implementation for JavaScript time. The point is to avoid casting from `double` to
// `int64_t` where possible and still be able to use `clock_cast` when needed. The epochs are the
// same so not much work is needed.
export class js_clock {
	public:
		using rep = double;
		using period = std::milli;
		using duration = std::chrono::duration<rep, period>;
		using time_point = std::chrono::time_point<js_clock>;
		const static bool is_steady = false;

		static auto now() noexcept -> time_point;
};

template <>
struct tag_for<js_clock::time_point> {
		using type = date_tag;
};

} // namespace ivm::value

namespace std::chrono {

using namespace ivm;

template <>
struct clock_time_conversion<utc_clock, value::js_clock> {
		auto operator()(value::js_clock::time_point time) const -> utc_clock::time_point {
			return utc_clock::time_point{duration_cast<utc_clock::duration>(time.time_since_epoch())};
		}
};

template <>
struct clock_time_conversion<value::js_clock, utc_clock> {
		auto operator()(utc_clock::time_point time) const -> value::js_clock::time_point {
			return value::js_clock::time_point{duration_cast<value::js_clock::duration>(time.time_since_epoch())};
		}
};

} // namespace std::chrono

namespace ivm::value {

auto js_clock::now() noexcept -> time_point {
	return clock_cast<js_clock>(std::chrono::utc_clock::now());
};

} // namespace ivm::value
