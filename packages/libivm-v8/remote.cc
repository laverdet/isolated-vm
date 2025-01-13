module;
#include <cassert>
#include <memory>
module isolated_v8.remote;
import v8;
import ivm.utility;

namespace isolated_v8 {

// remote_handle
remote_handle::remote_handle(
	remote_handle_delegate_lock& /*lock*/,
	std::weak_ptr<remote_handle_delegate> delegate,
	remote_handle_list& list,
	v8::Isolate* isolate,
	v8::Local<v8::Data> handle
) :
		delegate_{std::move(delegate)},
		global_{isolate, handle} {
	list.insert(*this);
}

remote_handle::~remote_handle() {
	assert(global_.IsEmpty());
}

auto remote_handle::deref(remote_handle_lock& /*lock*/, v8::Isolate* isolate) -> v8::Local<v8::Data> {
	return global_.Get(isolate);
}

auto remote_handle::reset_and_erase(remote_handle_lock& lock, remote_handle_list& list) -> void {
	reset_only(lock);
	list.erase(*this);
}

auto remote_handle::reset_only(remote_handle_lock& /*lock*/) -> void {
	global_.Reset();
	delegate_.reset();
}

// remote_handle_list
auto remote_handle_list::erase(remote_handle& handle) -> void {
	auto lock = list_.write();
	erase(lock, handle);
}

auto remote_handle_list::erase(write_lock_type& lock, remote_handle& handle) -> void {
	lock->erase(list_type::s_iterator_to(handle));
}

auto remote_handle_list::insert(remote_handle& handle) -> void {
	auto lock = list_.write();
	insert(lock, handle);
}

auto remote_handle_list::insert(write_lock_type& lock, remote_handle& handle) -> void {
	lock->push_back(handle);
}

auto remote_handle_list::reset(remote_handle_lock& lock) -> void {
	auto list_lock = list_.write();
	for (auto& handle : *list_lock) {
		handle.reset_only(lock);
	}
	list_lock->clear();
}

} // namespace isolated_v8
