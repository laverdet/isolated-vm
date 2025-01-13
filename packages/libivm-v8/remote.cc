module ivm.isolated_v8;
import :remote;
import v8;
import ivm.utility;

namespace isolated_v8 {

// remote_handle
remote_handle::remote_handle(agent::lock& agent_lock, v8::Local<v8::Data> handle) :
		foreground_runner_{agent_lock->foreground_runner()},
		handle_list_{agent_lock->remote_handle_list()},
		global_{agent_lock->isolate(), handle} {
	handle_list_->insert(*this);
}

remote_handle::~remote_handle() {
	handle_list_->erase(*this);
}

auto remote_handle::deref(agent::lock& agent_lock) -> v8::Local<v8::Data> {
	return global_.Get(agent_lock->isolate());
}

auto remote_handle::reset(agent::lock& /*lock*/) -> void {
	global_.Reset();
}

// remote_handle_list
auto remote_handle_list::erase(remote_handle& handle) -> void {
	list_.write()->erase(list_type::s_iterator_to(handle));
}

auto remote_handle_list::insert(remote_handle& handle) -> void {
	list_.write()->push_back(handle);
}

auto remote_handle_list::reset(agent::lock& lock) -> void {
	for (auto& handle : *list_.write()) {
		handle.reset(lock);
	}
}

} // namespace isolated_v8
