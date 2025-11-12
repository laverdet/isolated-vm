module;
#include <chrono>
#include <memory>
#include <stop_token>
export module ivm.utility:platform.timer;

namespace util {

class timer_thread;

// Manages a `std::stop_token` which is stopped after a duration, using a background thread.
export class timer_stop_token {
	public:
		using clock_type = std::chrono::steady_clock;

		explicit timer_stop_token(clock_type::duration duration);

		template <class Rep, class Period>
		explicit timer_stop_token(std::chrono::duration<Rep, Period> duration) :
				timer_stop_token(std::chrono::duration_cast<clock_type::duration>(duration)) {}

		timer_stop_token(const timer_stop_token&) = delete;
		timer_stop_token(timer_stop_token&&) noexcept = default;
		~timer_stop_token() = default;
		auto operator=(const timer_stop_token&) -> timer_stop_token& = delete;
		auto operator=(timer_stop_token&&) -> timer_stop_token& = delete;

		[[nodiscard]] auto get_source() const -> const std::stop_source&;
		[[nodiscard]] auto get_token() const -> std::stop_token;

	private:
		std::shared_ptr<timer_thread> thread_;
		std::stop_source stop_source_;
};

} // namespace util
