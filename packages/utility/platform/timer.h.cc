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
		timer_stop_token(std::defer_lock_t defer, duration_type duration);

		~timer_stop_token();

		[[nodiscard]] constexpr auto get_token() const noexcept -> inplace_stop_token { return source_.get_token(); }
		[[nodiscard]] constexpr auto stop_requested() const noexcept -> bool { return source_.stop_requested(); }
		constexpr auto operator==(const timer_stop_token& other) const noexcept -> bool { return this == &other; }
		auto pause() -> void;
		auto resume() -> void;
		// nb: Estimate only, per-thread timers may vary wildly
		[[nodiscard]] auto remaining() const -> duration_type;
		auto adjust(duration_type duration) -> bool;
		auto set(duration_type duration) -> bool;

	private:
		friend timer_thread;
		auto reset(const auto& update) -> bool;

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
