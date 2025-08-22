module;
#include <boost/intrusive/list.hpp>
#include <functional>
#include <memory>
export module isolated_v8:remote_handle;
import :remote_handle_list_fwd;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

export class remote_handle;
export using expired_remote_type = std::unique_ptr<remote_handle, auto (*)(remote_handle*)->void>;
export using reset_handle_type = std::function<auto(expired_remote_type)->void>;

// Interface needed in order to create `remote<T>` handles
export class remote_handle_lock {
	protected:
		remote_handle_lock(std::shared_ptr<remote_handle_list> list, reset_handle_type expiry_scheduler_);

	public:
		[[nodiscard]] auto handle_list() const -> auto& { return *list_; }
		[[nodiscard]] auto remote_expiration_task() const -> auto& { return expiry_scheduler_; }

	private:
		std::shared_ptr<remote_handle_list> list_;
		reset_handle_type expiry_scheduler_;
};

using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

// Base class for `remote<T>`. It contains a `v8::Global`, a reset delegate function, and an
// intrusive list hook for use in `remote_handle_list`.
class remote_handle : util::non_moveable {
	public:
		remote_handle(v8::Global<v8::Data> handle, reset_handle_type reset);
		auto reset(const js::iv8::isolate_lock_witness& lock) -> void;

	protected:
		auto deref(const js::iv8::isolate_lock_witness& lock) -> v8::Local<v8::Data>;
		static auto expire(expired_remote_type remote) -> void;

	private:
		v8::Global<v8::Data> global_;
		reset_handle_type reset_;
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<remote_handle, intrusive_list_hook, &remote_handle::hook_>;
};

} // namespace isolated_v8
