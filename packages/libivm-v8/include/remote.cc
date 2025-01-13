module;
#include <boost/intrusive/list.hpp>
#include <cstring>
#include <memory>
export module isolated_v8.remote;
import isolated_v8.foreground_runner;
import isolated_v8.scheduler;
import ivm.utility;
import v8;

namespace isolated_v8 {

using intrusive_no_size = boost::intrusive::constant_time_size<false>;
using intrusive_normal_mode = boost::intrusive::link_mode<boost::intrusive::normal_link>;
using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_normal_mode>;

class remote_handle;
export class remote_handle_list;

// Interface to reset a remote handle, supposedly on the isolate's foreground thread.
export class remote_handle_delegate : util::non_moveable {
	public:
		using expired_remote_type = std::unique_ptr<remote_handle, void (*)(remote_handle*)>;

	protected:
		~remote_handle_delegate() = default;

	public:
		virtual auto reset_remote_handle(expired_remote_type remote) -> void = 0;
};

// Interface to make a remote handle. It is supposed to prove a v8 lock and also provides
// bookkeeping references.
export class remote_handle_lock : util::non_moveable {
	protected:
		~remote_handle_lock() = default;

	public:
		[[nodiscard]] virtual auto isolate() -> v8::Isolate* = 0;
		[[nodiscard]] virtual auto remote_handle_list() -> remote_handle_list& = 0;
};

export class remote_handle_delegate_lock : public remote_handle_lock {
	protected:
		~remote_handle_delegate_lock() = default;

	public:
		[[nodiscard]] virtual auto remote_handle_delegate() -> std::weak_ptr<remote_handle_delegate> = 0;
};

// Base class for `remote<T>`. Performs insertion and removal from the agent storage, and the
// underlying v8 handle.
class remote_handle : util::non_moveable {
	private:
		template <class> friend class remote;
		friend remote_handle_list;

		remote_handle(
			remote_handle_delegate_lock& lock,
			std::weak_ptr<remote_handle_delegate> delegate,
			remote_handle_list& list,
			v8::Isolate* isolate,
			v8::Local<v8::Data> handle
		);

	protected:
		remote_handle(remote_handle_delegate_lock& lock, v8::Local<v8::Data> handle) :
				remote_handle{lock, lock.remote_handle_delegate(), lock.remote_handle_list(), lock.isolate(), handle} {}
		~remote_handle();

		auto deref(remote_handle_lock& lock) -> v8::Local<v8::Data> { return deref(lock, lock.isolate()); }

	public:
		auto reset(remote_handle_lock& lock) -> void { reset_and_erase(lock, lock.remote_handle_list()); }

	private:
		auto deref(remote_handle_lock& lock, v8::Isolate* isolate) -> v8::Local<v8::Data>;
		auto reset_and_erase(remote_handle_lock& lock, remote_handle_list& list) -> void;
		auto reset_only(remote_handle_lock& lock) -> void;

		std::weak_ptr<remote_handle_delegate> delegate_;
		v8::Global<v8::Data> global_;
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<remote_handle, intrusive_list_hook, &remote_handle::hook_>;
};

// Reference to a persistent in v8. It should always be created with `remote<T>::make_shared` or
// `remote<T>::make_unique`. When the smart pointer is released the handle schedules a task to reset
// itself in the isolate thread.
export template <class Type>
class remote : public remote_handle {
	private:
		struct private_ctor {
				explicit private_ctor() = default;
		};
		// Delete the type-erased `remote_handle` instance by `remote<T>::~remote()`
		static auto delete_concrete_instance(remote_handle* self) -> void;
		// Invoked when the smart pointer expires. This erases the type into a `expired_remote_type` and
		// sends it to the delegate.
		static auto send(remote* self) -> void;

	public:
		using unique_remote = std::unique_ptr<remote, util::functor_of<send>>;
		remote(private_ctor /*private*/, remote_handle_delegate_lock& lock, v8::Local<v8::Data> handle) :
				remote_handle{lock, handle} {}

		auto deref(remote_handle_lock& lock) -> v8::Local<Type>;
		static auto make_shared(remote_handle_delegate_lock& lock, v8::Local<Type> handle) -> std::shared_ptr<remote>;
		static auto make_unique(remote_handle_delegate_lock& lock, v8::Local<Type> handle) -> unique_remote;
};

// Non-owning intrusive list of all outstanding remote handles. Before destruction of the isolate
// all handles can be reset.
class remote_handle_list {
	private:
		friend remote_handle;
		using list_type = boost::intrusive::list<remote_handle, intrusive_no_size, remote_handle::hook_type>;
		using lockable_list_type = util::lockable<list_type>;
		using write_lock_type = lockable_list_type::write_type;

	public:
		auto erase(remote_handle& handle) -> void;
		auto insert(remote_handle& handle) -> void;
		auto reset(remote_handle_lock& lock) -> void;

	private:
		static auto erase(write_lock_type& lock, remote_handle& handle) -> void;
		static auto insert(write_lock_type& lock, remote_handle& handle) -> void;

		lockable_list_type list_;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = remote<Type>::unique_remote;

export template <class Type>
auto make_shared_remote(remote_handle_delegate_lock& lock, v8::Local<Type> handle) -> shared_remote<Type> {
	return remote<Type>::make_shared(lock, handle);
}

// ---

template <class Type>
auto remote<Type>::delete_concrete_instance(remote_handle* self) -> void {
	delete static_cast<remote<Type>*>(self);
}

template <class Type>
auto remote<Type>::deref(remote_handle_lock& lock) -> v8::Local<Type> {
	// `handle.As<Type>()` doesn't work because handles like `UnboundScript` don't provide a `Cast`
	// function even though they are `Data`.
	auto data = remote_handle::deref(lock);
	v8::Local<Type> local;
	static_assert(sizeof(data) == sizeof(local));
	std::memcpy(&local, static_cast<void*>(&data), sizeof(local));
	return local;
}

template <class Type>
auto remote<Type>::make_shared(remote_handle_delegate_lock& lock, v8::Local<Type> handle) -> std::shared_ptr<remote> {
	return std::shared_ptr<remote>{new remote{private_ctor{}, lock, handle}, send};
}

template <class Type>
auto remote<Type>::make_unique(remote_handle_delegate_lock& lock, v8::Local<Type> handle) -> unique_remote {
	return unique_remote{new remote{private_ctor{}, lock, handle}};
}

template <class Type>
auto remote<Type>::send(remote* self) -> void {
	remote_handle_delegate::expired_remote_type ptr{self, delete_concrete_instance};
	auto delegate = ptr->delegate_.lock();
	if (delegate) {
		delegate->reset_remote_handle(std::move(ptr));
	} else {
		// nb: Technically a data race since this would be from an unknown thread.
		assert(ptr->global_.IsEmpty());
	}
}

} // namespace isolated_v8
