module;
#include <boost/intrusive/list.hpp>
export module isolated_v8.remote_handle_list;
import isolated_v8.remote_handle;
import isolated_v8.lock;
import ivm.utility;
import v8;

namespace isolated_v8 {

using intrusive_no_size = boost::intrusive::constant_time_size<false>;

// Non-owning intrusive list of all outstanding remote handles. Before destruction of the isolate
// all handles can be reset.
export class remote_handle_list {
	private:
		using list_type = boost::intrusive::list<remote_handle, intrusive_no_size, remote_handle::hook_type>;
		using lockable_list_type = util::lockable<list_type>;
		using write_lock_type = lockable_list_type::write_type;

	public:
		auto erase(remote_handle& handle) -> void;
		auto insert(remote_handle& handle) -> void;
		auto reset(const isolate_lock& lock) -> void;

	private:
		lockable_list_type list_;
};

} // namespace isolated_v8
