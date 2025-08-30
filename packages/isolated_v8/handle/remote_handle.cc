module;
#include <memory>
#include <utility>
module isolated_v8;
import :remote_handle;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// remote_handle_lock
remote_handle_lock::remote_handle_lock(remote_handle_list& list, reset_handle_type expiry_scheduler_) :
		list_{list},
		expiry_scheduler_{std::move(expiry_scheduler_)} {}

// remote_handle
remote_handle::remote_handle(v8::Global<v8::Data> handle, reset_handle_type reset) :
		global_{std::move(handle)},
		reset_{std::move(reset)} {}

auto remote_handle::deref(const js::iv8::isolate_lock_witness& lock) -> v8::Local<v8::Data> {
	return global_.Get(lock.isolate());
}

auto remote_handle::reset(const js::iv8::isolate_lock_witness& /*lock*/) -> void {
	global_.Reset();
}

auto remote_handle::expire(expired_remote_type remote) -> void {
	util::move_pointer_operation(std::move(remote), [ & ](auto* ptr, auto unique) {
		ptr->reset_(std::move(unique));
	});
}

} // namespace isolated_v8
