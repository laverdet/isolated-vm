module;
#include <boost/intrusive/list.hpp>
#include <functional>
#include <memory>
export module v8_js:remote_handle;
import :lock;
import ivm.utility;
import v8;

namespace js::iv8 {

// forward declarations
class remote_handle;

export using expired_remote_type = std::unique_ptr<remote_handle>;
export using reset_handle_type = util::function_ref<auto(expired_remote_type) noexcept -> void>;

// Storage class for `remote<T>`. It contains a `v8::Global`, and an intrusive list hook for use in
// `remote_handle_list`. `remote<T>` maintains a smart pointer to this class.
class remote_handle : util::non_copyable {
	public:
		class expire;
		using shared_type = std::shared_ptr<remote_handle>;
		using unique_type = std::unique_ptr<remote_handle, expire>;

		template <class Type>
		explicit remote_handle(isolate_lock_witness lock, v8::Local<Type> local);

		template <class Type>
		[[nodiscard]] auto deref(isolate_lock_witness lock, std::type_identity<Type> type) const -> v8::Local<Type>;

		auto reset(isolate_lock_witness lock) -> void;

	private:
		explicit remote_handle(isolate_lock_witness lock, v8::Local<v8::Data> handle);
		[[nodiscard]] auto deref(isolate_lock_witness lock) const -> v8::Local<v8::Data>;

		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

		v8::Global<v8::Data> global_;
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<remote_handle, intrusive_list_hook, &remote_handle::hook_>;
};

class remote_handle::expire {
	public:
		explicit expire(std::weak_ptr<reset_handle_type> reset);
		auto operator()(remote_handle* self) const noexcept -> void;

	private:
		std::weak_ptr<reset_handle_type> reset_;
};

// Non-owning intrusive list of all outstanding remote handles. Before destruction of the isolate
// all handles can be reset.
export class remote_handle_list {
	private:
		using intrusive_no_size = boost::intrusive::constant_time_size<false>;
		using list_type = boost::intrusive::list<remote_handle, intrusive_no_size, remote_handle::hook_type>;

	public:
		auto clear(isolate_lock_witness lock) -> void;
		auto erase(remote_handle& handle) -> void;
		auto insert(remote_handle& handle) -> void;

	private:
		util::lockable<list_type> list_;
};

// Lock interface required to create `remote<T>` handles.
export class remote_handle_lock {
	public:
		remote_handle_lock(isolate_lock_witness witness, remote_handle_list& list, std::weak_ptr<reset_handle_type> expiry_scheduler);
		[[nodiscard]] auto handle_list() const -> auto& { return list_.get(); }
		[[nodiscard]] auto expiry_scheduler() const -> auto& { return expiry_scheduler_; }
		[[nodiscard]] auto witness() const -> isolate_lock_witness;

	private:
		std::reference_wrapper<remote_handle_list> list_;
		std::weak_ptr<reset_handle_type> expiry_scheduler_;
		v8::Isolate* isolate_;
};

// ---

template <class Type>
remote_handle::remote_handle(isolate_lock_witness lock, v8::Local<Type> local) :
		remote_handle{lock, std::bit_cast<v8::Local<v8::Data>>(local)} {}

template <class Type>
auto remote_handle::deref(isolate_lock_witness lock, std::type_identity<Type> /*type*/) const -> v8::Local<Type> {
	// `handle.As<Type>()` doesn't work because types like `UnboundScript` don't provide a `Cast`
	// function.
	return std::bit_cast<v8::Local<Type>>(deref(lock));
}

} // namespace js::iv8
