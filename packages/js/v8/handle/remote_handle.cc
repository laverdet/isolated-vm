module;
#include <memory>
#include <utility>
module v8_js;
import ivm.utility;

namespace js::iv8 {

// remote_handle
remote_handle::remote_handle(isolate_lock_witness lock, v8::Local<v8::Data> handle) :
		global_{lock.isolate(), handle} {}

auto remote_handle::reset(isolate_lock_witness /*lock*/) -> void {
	global_.Reset();
}

auto remote_handle::deref(isolate_lock_witness lock) const -> v8::Local<v8::Data> {
	return global_.Get(lock.isolate());
}

// remote_handle::expire
remote_handle::expire::expire(std::weak_ptr<reset_handle_type> reset) :
		reset_{std::move(reset)} {
}

auto remote_handle::expire::operator()(remote_handle* self) const noexcept -> void {
	auto handle = std::unique_ptr<remote_handle>{self};
	if (auto reset = reset_.lock()) {
		(*reset)(std::move(handle));
	}
}

// remote_handle_list
auto remote_handle_list::erase(remote_handle& handle) -> void {
	list_.write()->erase(list_type::s_iterator_to(handle));
}

auto remote_handle_list::insert(remote_handle& handle) -> void {
	list_.write()->push_back(handle);
}

auto remote_handle_list::clear(isolate_lock_witness lock) -> void {
	auto list_lock = list_.write();
	for (auto& handle : *list_lock) {
		handle.reset(lock);
	}
	list_lock->clear();
}

// remote_handle_lock
remote_handle_lock::remote_handle_lock(isolate_lock_witness witness, remote_handle_list& list, std::weak_ptr<reset_handle_type> expiry_scheduler_) :
		list_{list},
		expiry_scheduler_{std::move(expiry_scheduler_)},
		isolate_{witness.isolate()} {}

auto remote_handle_lock::witness() const -> isolate_lock_witness {
	return isolate_lock_witness::make_witness(isolate_);
}

} // namespace js::iv8
