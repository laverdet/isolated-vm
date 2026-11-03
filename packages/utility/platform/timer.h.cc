export module util:platform.timer;
import :platform.inplace_stop_token;
import :utility;
import boost;
import std;

namespace util {

class timer_thread;

// `inplace_stop_source` which requests stop after a duration has elapsed.
export class timer_stop_token : util::non_moveable {
	public:
		using clock_type = std::chrono::steady_clock;
		using duration_type = clock_type::duration;

		explicit timer_stop_token(duration_type duration);
		template <class Rep, class Period>
		explicit constexpr timer_stop_token(std::chrono::duration<Rep, Period> duration) :
				timer_stop_token{[](auto duration) {
					using input_type = std::chrono::duration<Rep, Period>;
					if constexpr (std::floating_point<Rep> || std::ratio_greater_equal_v<Period, duration_type::period>) {
						if (!(duration < std::chrono::floor<input_type>(duration_type::max()))) {
							return duration_type::max();
						}
					}
					if (duration <= input_type::zero()) {
						return duration_type::zero();
					}
					return std::chrono::ceil<duration_type>(duration);
				}(duration)} {}
		~timer_stop_token();

		[[nodiscard]] constexpr auto get_token() const noexcept -> inplace_stop_token { return source_.get_token(); }
		[[nodiscard]] constexpr auto stop_requested() const noexcept -> bool { return source_.stop_requested(); }
		constexpr auto operator==(const timer_stop_token& other) const noexcept -> bool { return this == &other; }
		auto pause() -> void;
		auto resume() -> void;

	private:
		friend timer_thread;
		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

		intrusive_list_hook list_hook_;
		std::shared_ptr<timer_thread> thread_;
		util::inplace_stop_source source_;
		duration_type duration_;
		clock_type::time_point time_point_ = clock_type::time_point::min();

	public:
		using intrusive_hook = boost::intrusive::member_hook<timer_stop_token, intrusive_list_hook, &timer_stop_token::list_hook_>;
};

} // namespace util
