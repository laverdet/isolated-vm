module util;
import :platform.inplace_stop_token;
import std;

namespace util {

// `inplace_stop_callback_base`
inplace_stop_callback_base::inplace_stop_callback_base(inplace_stop_token token, invoke_type invoke) :
		source_{token.source_},
		invoke_{invoke} {}

auto inplace_stop_callback_base::attach() noexcept -> void {
	if (source_ != nullptr && !try_attach()) {
		invoke_();
	}
}

auto inplace_stop_callback_base::try_attach() noexcept -> bool {
	if (source_ == nullptr) {
		return false;
	}
	auto lock = std::scoped_lock{source_->mutex_};
	if (source_->stop_requested()) {
		return false;
	}
	auto& head = source_->callbacks_;
	prev_ = &head;
	next_ = head.next_;
	if (next_ != nullptr) {
		next_->prev_ = this;
	}
	head.next_ = this;
	return true;
}

auto inplace_stop_callback_base::detach() noexcept -> void {
	if (source_ == nullptr) {
		return;
	}
	{
		auto lock = std::scoped_lock{source_->mutex_};
		if (prev_ != nullptr) {
			unlink();
			return;
		}
	}
	// `request_stop` unlinked this callback.
	if (source_->requesting_thread_ != std::this_thread::get_id()) {
		while (source_->executing_.load(std::memory_order_acquire) == this) {
			std::this_thread::yield();
		}
	}
}

auto inplace_stop_callback_base::unlink() noexcept -> void {
	prev_->next_ = next_;
	if (next_ != nullptr) {
		next_->prev_ = prev_;
	}
	prev_ = nullptr;
}

// `inplace_stop_source`
auto inplace_stop_source::get_token() const noexcept -> inplace_stop_token {
	return inplace_stop_token{this};
}

auto inplace_stop_source::request_stop() noexcept -> bool {
	if (stop_requested_.exchange(true, std::memory_order_acq_rel)) {
		return false;
	}
	requesting_thread_ = std::this_thread::get_id();
	auto lock = std::unique_lock{mutex_};
	while (callbacks_.next_ != nullptr) {
		auto* callback = callbacks_.next_;
		callback->unlink();
		executing_.store(callback, std::memory_order_release);
		lock.unlock();
		callback->invoke_();
		executing_.store(nullptr, std::memory_order_release);
		lock.lock();
	}
	return true;
}

} // namespace util
