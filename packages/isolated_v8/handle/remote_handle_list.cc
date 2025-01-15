module isolated_v8.remote_handle_list;
import isolated_v8.lock;
import isolated_v8.remote_handle;
import v8;

namespace isolated_v8 {

auto remote_handle_list::erase(remote_handle& handle) -> void {
	list_.write()->erase(list_type::s_iterator_to(handle));
}

auto remote_handle_list::insert(remote_handle& handle) -> void {
	list_.write()->push_back(handle);
}

auto remote_handle_list::reset(const isolate_lock& lock) -> void {
	auto list_lock = list_.write();
	for (auto& handle : *list_lock) {
		handle.reset(lock);
	}
	list_lock->clear();
}

} // namespace isolated_v8
