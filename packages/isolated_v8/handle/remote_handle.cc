module;
#include <utility>
module isolated_v8.remote_handle;
import isolated_v8.lock;
import v8;

namespace isolated_v8 {

remote_handle::remote_handle(v8::Global<v8::Data> handle, reset_handle_type reset) :
		global_{std::move(handle)},
		reset_{std::move(reset)} {}

auto remote_handle::deref(const isolate_lock& lock) -> v8::Local<v8::Data> {
	return global_.Get(lock.isolate());
}

auto remote_handle::reset(const isolate_lock& /*lock*/) -> void {
	global_.Reset();
}

auto remote_handle::expire(expired_remote_type remote) -> void {
	auto* ptr = remote.get();
	ptr->reset_(std::move(remote));
}

} // namespace isolated_v8
