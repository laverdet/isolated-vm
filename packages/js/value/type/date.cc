module;
#include <chrono>
export module isolated_js.date;
#ifdef _LIBCPP_VERSION
namespace std::chrono {
using utc_clock = system_clock;
}
#endif

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
		const static bool is_steady = false;

		static auto now() noexcept -> time_point;
};

auto js_clock::now() noexcept -> time_point {
	auto time_ms = duration_cast<js_clock::duration>(std::chrono::utc_clock::now().time_since_epoch());
	return time_point{time_ms};
};

} // namespace js
