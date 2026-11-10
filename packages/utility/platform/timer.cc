module util;
import :platform.inplace_stop_token;
import :platform.lockable;
import :platform.timer;
import :utility;
import boost;
import std;

namespace util {
namespace {
util::lockable_with<std::weak_ptr<timer_thread>, {.shared = true}> shared_thread;
}

class timer_thread : non_moveable {
	public:
		using clock_type = timer_stop_token::clock_type;
		using duration_type = timer_stop_token::duration_type;

		timer_thread();
		~timer_thread();

		constexpr static auto acquire() -> std::shared_ptr<timer_thread>;

	private:
		friend timer_stop_token;
		using intrusive_no_size = boost::intrusive::constant_time_size<false>;
		using list_type = boost::intrusive::list<timer_stop_token, intrusive_no_size, timer_stop_token::intrusive_hook>;

		struct storage {
				list_type active;
				list_type paused;

				constexpr auto insert(timer_stop_token& timer) -> void;
				auto list_of(timer_stop_token& timer) -> list_type&;
		};

		auto loop() -> void;

		util::lockable_with<storage, {.interuptable = true, .notifiable = true}> storage_;
		util::inplace_stop_source stop_source_;
		std::thread thread_;
};
// ---

// `timer_thread`
timer_thread::timer_thread() :
		thread_{&timer_thread::loop, this} {}

timer_thread::~timer_thread() {
	stop_source_.request_stop();
	thread_.join();
}

constexpr auto timer_thread::storage::insert(timer_stop_token& timer) -> void {
	auto it = std::ranges::find_if(std::views::reverse(active), [ & ](const timer_stop_token& that) -> bool {
		return that.time_point_ <= timer.time_point_;
	});
	active.insert(it.base(), timer);
}

auto timer_thread::storage::list_of(timer_stop_token& timer) -> list_type& {
	auto has_time_point =
		[](const timer_stop_token& timer) -> bool { return timer.time_point_ != clock_type::time_point::min(); };
	if (timer.time_point_ == clock_type::time_point::min()) {
		// Activation request or paused
		auto requests = active | std::views::take_while(std::not_fn(has_time_point));
		return std::ranges::find(requests, timer) == requests.end() ? paused : active;
	} else {
		// Pause request or waiting
		auto requests = paused | std::views::take_while(has_time_point);
		return std::ranges::find(requests, timer) == requests.end() ? active : paused;
	}
}

auto timer_thread::loop() -> void {
	const auto stop_token = stop_source_.get_token();
	auto lock = storage_.write_waitable([](const storage& storage) -> bool {
		return (
			(!storage.active.empty() && storage.active.front().time_point_ == clock_type::time_point::min()) ||
			(!storage.paused.empty() && storage.paused.front().time_point_ != clock_type::time_point::min())
		);
	});
	while (!stop_token.stop_requested()) {
		const auto now = clock_type::now();
		list_type will_fire;
		// Handle `active` list
		const auto deadline = [ & ] -> clock_type::time_point {
			while (!lock->active.empty()) {
				auto& timer = lock->active.front();
				if (timer.time_point_ == clock_type::time_point::min()) {
					// Calculate trigger timepoint and reinsert
					lock->active.pop_front();
					timer.time_point_ = now + std::min(timer.duration_, clock_type::time_point::max() - now);
					lock->insert(timer);
				} else if (timer.time_point_ <= now) {
					// Timer will fire
					lock->active.pop_front();
					timer.time_point_ = clock_type::time_point::min();
					timer.duration_ = duration_type::zero();
					will_fire.push_back(timer);
				} else {
					// Next timer to fire
					return timer.time_point_;
				}
			}
			return clock_type::time_point::max();
		}();
		// Handle `paused` list
		while (!lock->paused.empty()) {
			auto& timer = lock->paused.front();
			if (timer.time_point_ == clock_type::time_point::min()) {
				// Timer has already been paused
				break;
			} else {
				// Recalculate duration
				lock->paused.pop_front();
				timer.duration_ = timer.time_point_ - now;
				timer.time_point_ = clock_type::time_point::min();
				if (timer.duration_ <= duration_type::zero()) {
					timer.duration_ = duration_type::zero();
					will_fire.push_back(timer);
				} else {
					lock->paused.push_back(timer);
				}
			}
		}
		// Trigger expired timers
		if (!will_fire.empty()) {
			lock.unlock();
			for (auto& timer : will_fire) {
				timer.source_.request_stop();
			}
			lock.lock();
			will_fire.clear();
		}
		// Sleep until next expiration or request
		if (deadline == clock_type::time_point::max()) {
			lock.wait(stop_token);
		} else {
			lock.wait(stop_token, deadline);
		}
	}
}

constexpr auto timer_thread::acquire() -> std::shared_ptr<timer_thread> {
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
timer_stop_token::timer_stop_token(duration_type duration) :
		thread_{timer_thread::acquire()},
		duration_{std::max(duration, duration_type::zero())} {
	if (duration_ == duration_type::zero()) {
		source_.request_stop();
	} else {
		thread_->storage_.write_notify()->active.push_front(*this);
	}
}

timer_stop_token::timer_stop_token(std::defer_lock_t /*defer*/, duration_type duration) :
		thread_{timer_thread::acquire()},
		duration_{std::max(duration, duration_type::zero())} {
	if (duration_ == duration_type::zero()) {
		source_.request_stop();
	} else {
		thread_->storage_.write()->paused.push_back(*this);
	}
}

timer_stop_token::~timer_stop_token() {
	auto expired = [ & ] -> bool {
		auto lock = thread_->storage_.write();
		if (duration_ == duration_type::zero()) {
			// Already expired
			return true;
		}
		lock->list_of(*this).erase(timer_thread::list_type::s_iterator_to(*this));
		return false;
	}();
	// Spin until the thread is done with this timer.
	if (expired) {
		auto is_linked = [ & ] -> bool {
			auto lock = thread_->storage_.read();
			return list_hook_.is_linked();
		};
		while (is_linked()) {
			std::this_thread::yield();
		}
	}
}

auto timer_stop_token::pause() -> void {
	auto lock = thread_->storage_.write_notify();
	if (duration_ == duration_type::zero()) {
		// Already expired
		return;
	}
	lock->active.erase(timer_thread::list_type::s_iterator_to(*this));
	if (time_point_ == clock_type::time_point::min()) {
		// Never activated
		lock->paused.push_back(*this);
	} else {
		// Request remaining duration
		lock->paused.push_front(*this);
	}
}

auto timer_stop_token::remaining() const -> duration_type {
	auto lock = thread_->storage_.read();
	if (duration_ == duration_type::zero()) {
		// Already expired
		return duration_type::zero();
	} else if (time_point_ == clock_type::time_point::min()) {
		// Paused, or waiting for activation
		return duration_;
	} else {
		return std::max(duration_type::zero(), time_point_ - clock_type::now());
	}
}

auto timer_stop_token::adjust(duration_type duration) -> bool {
	return reset([ & ](duration_type remaining) -> duration_type {
		// See: `std::saturating_add`
		if (duration > duration_type::zero()) {
			return remaining > duration_type::max() - duration ? duration_type::max() : remaining + duration;
		} else {
			return remaining < duration_type::min() - duration ? duration_type::min() : remaining + duration;
		}
	});
}

auto timer_stop_token::set(duration_type duration) -> bool {
	return reset([ & ](duration_type /*remaining*/) -> duration_type { return duration; });
}

auto timer_stop_token::reset(const auto& update) -> bool {
	enum class outcome : std::uint8_t {
		expired,
		updated,
		expire
	};
	auto result = [ & ] -> outcome {
		auto lock = thread_->storage_.write_notify();
		if (duration_ == duration_type::zero()) {
			// Already expired
			return outcome::expired;
		}
		auto& list = lock->list_of(*this);
		auto remaining = time_point_ == clock_type::time_point::min()
			? duration_
			: std::max(duration_type::zero(), time_point_ - clock_type::now());
		list.erase(timer_thread::list_type::s_iterator_to(*this));
		duration_ = std::max(update(remaining), duration_type::zero());
		time_point_ = clock_type::time_point::min();
		if (duration_ == duration_type::zero()) {
			return outcome::expire;
		} else if (&list == &lock->paused) {
			list.push_back(*this);
		} else {
			list.push_front(*this);
		}
		return outcome::updated;
	}();
	if (result == outcome::expire) {
		source_.request_stop();
	}
	return result != outcome::expired;
}

auto timer_stop_token::resume() -> void {
	auto lock = thread_->storage_.write_notify();
	if (duration_ == duration_type::zero()) {
		// Already expired
		return;
	}
	lock->paused.erase(timer_thread::list_type::s_iterator_to(*this));
	if (time_point_ == clock_type::time_point::min()) {
		// Request a new time point
		lock->active.push_front(*this);
	} else {
		// Never deactivated
		lock->insert(*this);
	}
}

} // namespace util
