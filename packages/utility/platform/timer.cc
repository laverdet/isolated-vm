module;
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>
module ivm.utility;
import :functional;
import :lockable;
import :timer;

namespace util {

namespace {
util::lockable_with<std::weak_ptr<timer_thread>, {.shared = true}> shared_thread;
}

// Internal thread which manages timers
class timer_thread : util::non_moveable {
	public:
		class timer;
		using clock_type = timer_stop_token::clock_type;

		timer_thread();
		auto insert(std::stop_source stop_source, clock_type::duration timeout) -> void;
		static auto acquire() -> std::shared_ptr<timer_thread>;

	private:
		using timer_queue_type = std::vector<timer>;
		using request_type = util::function_ref<auto(timer_queue_type&)->void>;
		using request_queue_type = std::vector<request_type>;

		template <class Invocable>
		auto dispatch(Invocable invocable, auto&&... args);

		auto dispatch(request_type request) -> void;
		auto loop(std::stop_token stop_token) -> void;

		util::lockable_with<request_queue_type, {.interuptable = true, .shared = true}> requests_;
		timer_queue_type timers_;
		std::jthread thread_;
};

// Internal thread-managed timer
class timer_thread::timer : public util::pointer_facade {
	public:
		explicit timer(clock_type::time_point time_point, std::stop_source&& stop_source) :
				time_point_{time_point},
				stop_source_{std::move(stop_source)} {}

		auto operator*() & -> std::stop_source& { return stop_source_; }
		auto operator*() const& -> const std::stop_source& { return stop_source_; }

		[[nodiscard]] auto time_point() const -> clock_type::time_point { return time_point_; }

	private:
		clock_type::time_point time_point_;
		std::stop_source stop_source_;
};

// ---

// `timer_thread`
timer_thread::timer_thread() :
		thread_{[ & ](std::stop_token stop_token) -> void { loop(stop_token); }} {}

auto timer_thread::loop(std::stop_token stop_token) -> void {
	auto lock = requests_.write_waitable(std::not_fn(&request_queue_type::empty));
	while (!stop_token.stop_requested()) {
		// wait for requests or timeout
		const auto has_requests = [ & ]() -> bool {
			if (timers_.empty()) {
				return lock.wait(stop_token);
			} else {
				auto next = timers_.back().time_point();
				return lock.wait(stop_token, next);
			}
		}();
		// dispatch timer requests
		if (has_requests) {
			for (const auto& request : *lock) {
				request(timers_);
			}
			lock->clear();
			lock.notify_all();
		}
		// expire timers
		auto now = clock_type::now();
		auto expired_timer_view =
			std::ranges::reverse_view{timers_} |
			std::views::take_while([ & ](const auto& timer) {
				return timer.time_point() <= now;
			});
		for (auto& timer : expired_timer_view) {
			timer->request_stop();
			timers_.pop_back();
		}
	}
}

auto timer_thread::dispatch(request_type request) -> void {
	requests_.write_notify()->push_back(request);
	auto does_not_contain = [ & ](const auto& queue) -> bool {
		return !std::ranges::contains(queue, request);
	};
	requests_.read_waitable(does_not_contain).wait();
}

template <class Invocable>
auto timer_thread::dispatch(Invocable invocable, auto&&... args) {
	auto implementation = util::regular_return{
		[ invocable = std::move(invocable),
			... args = std::forward<decltype(args)>(args) ](
			timer_queue_type& queue
		) mutable -> void {
			return invocable(queue, std::forward<decltype(args)>(args)...);
		}
	};
	using result_type = std::invoke_result_t<decltype(implementation), timer_queue_type&>;
	std::optional<result_type> result;
	auto request = [ & ](timer_queue_type& queue) -> void {
		result = implementation(queue);
	};
	dispatch(request_type{request});
	return *std::move(result).value_or(util::elide{[]() -> result_type {
		std::unreachable();
	}});
}

auto timer_thread::insert(std::stop_source stop_source, clock_type::duration timeout) -> void {
	dispatch([ & ](timer_queue_type& queue) -> void {
		auto time_point = clock_type::now() + timeout;
		constexpr auto projection = [](const timer_thread::timer& timer) -> timer_thread::clock_type::time_point {
			return timer.time_point();
		};
		auto it = std::ranges::upper_bound(queue, time_point, std::greater{}, projection);
		queue.emplace(it, time_point, std::move(stop_source));
	});
}

auto timer_thread::acquire() -> std::shared_ptr<timer_thread> {
	auto acquired = shared_thread.read()->lock();
	if (!acquired) {
		auto lock = shared_thread.write();
		acquired = lock->lock();
		if (!acquired) {
			*lock = acquired = std::make_shared<timer_thread>();
		}
	}
	return acquired;
}

// `timer_stop_token`
timer_stop_token::timer_stop_token(clock_type::duration duration) :
		thread_{timer_thread::acquire()} {
	thread_->insert(stop_source_, duration);
}

auto timer_stop_token::operator*() const -> std::stop_token {
	return stop_source_.get_token();
}

} // namespace util
