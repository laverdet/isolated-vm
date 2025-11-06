module;
#include <boost/intrusive/list.hpp>
#include <functional>
#include <memory>
export module v8_js:remote;
import :lock;
import ivm.utility;
import v8;

namespace js::iv8 {

// Storage class for `remote<T>`. It contains a `v8::Global`, and an intrusive list hook for use in
// `remote_handle_list`.
class remote_handle : util::non_copyable {
	public:
		class expire;
		explicit remote_handle(isolate_lock_witness lock, v8::Local<v8::Data> handle);

		[[nodiscard]] auto deref(isolate_lock_witness lock) const -> v8::Local<v8::Data>;
		auto reset(isolate_lock_witness lock) -> void;

	private:
		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

		v8::Global<v8::Data> global_;
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<remote_handle, intrusive_list_hook, &remote_handle::hook_>;
};

// Required types for `remote_handle_lock`
export using expired_remote_type = std::unique_ptr<remote_handle, util::derived_delete<remote_handle>>;
export using reset_handle_type = util::function_ref<auto(expired_remote_type) noexcept -> void>;

// "Deleter" for `remote_handle` which actually just forwards the handle to a reset scheduler.
class remote_handle::expire {
	public:
		explicit expire(std::weak_ptr<reset_handle_type> reset);

		template <class Type>
		auto operator()(Type* pointer) const noexcept -> void;

	private:
		auto dispatch(remote_handle* pointer, util::derived_delete<remote_handle> delete_) const noexcept -> void;

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
		[[nodiscard]] auto witness() const -> isolate_lock_witness { return witness_; }

	private:
		std::reference_wrapper<remote_handle_list> list_;
		std::weak_ptr<reset_handle_type> expiry_scheduler_;
		isolate_lock_witness witness_;
};

// Reference to a persistent in v8. It should always be created with `make_shared_remote` or
// `make_unique_remote`. When the smart pointer is released the handle schedules a task to reset
// itself in the isolate thread.
export template <class Type>
class remote : public remote_handle {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};

	public:
		explicit remote(private_constructor /*private*/, isolate_lock_witness lock, v8::Local<Type> local);
		[[nodiscard]] auto deref(isolate_lock_witness lock) const -> v8::Local<Type>;
		static auto make(const remote_handle_lock& lock, v8::Local<Type> local) -> std::unique_ptr<remote, expire>;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = std::unique_ptr<remote<Type>, remote_handle::expire>;

export template <class Type>
auto make_shared_remote(const remote_handle_lock& lock, v8::Local<Type> local) -> shared_remote<Type> {
	return std::shared_ptr{remote<Type>::make(lock, local)};
}

export template <class Type>
auto make_unique_remote(const remote_handle_lock& lock, v8::Local<Type> local) -> unique_remote<Type> {
	return remote<Type>::make(lock, local);
}

// ---

// `remote_handle::expire`
template <class Type>
auto remote_handle::expire::operator()(Type* pointer) const noexcept -> void {
	// `remote<T>` and `remote_handle` have identical layouts, but deleting a `remote<T>` through
	// `remote_handle` is UB. So `util::derived_delete` takes the place of a virtual destructor.
	dispatch(pointer, util::derived_delete<remote_handle>{type<Type>});
}

// `remote<T>`
template <class Type>
remote<Type>::remote(private_constructor /*private*/, isolate_lock_witness lock, v8::Local<Type> local) :
		remote_handle{lock, std::bit_cast<v8::Local<v8::Data>>(local)} {}

template <class Type>
auto remote<Type>::deref(isolate_lock_witness lock) const -> v8::Local<Type> {
	// `handle.As<Type>()` doesn't work because types like `UnboundScript` don't provide a `Cast`
	// function.
	return std::bit_cast<v8::Local<Type>>(remote_handle::deref(lock));
}

template <class Type>
auto remote<Type>::make(const remote_handle_lock& lock, v8::Local<Type> local) -> std::unique_ptr<remote, expire> {
	auto handle_with_expire = unique_remote<Type>{nullptr, remote_handle::expire{lock.expiry_scheduler()}};
	auto handle = std::make_unique<remote>(private_constructor{}, lock.witness(), local);
	handle_with_expire.reset(handle.release());
	lock.handle_list().insert(*handle_with_expire);
	return handle_with_expire;
}

} // namespace js::iv8
