module;
#include <bit>
#include <memory>
export module isolated_v8.remote;
import isolated_v8.remote_handle;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// Reference to a persistent in v8. It should always be created with `remote<T>::make_shared` or
// `remote<T>::make_unique`. When the smart pointer is released the handle schedules a task to reset
// itself in the isolate thread.
export template <class Type>
class remote : public remote_handle {
	public:
		using unique_remote = std::unique_ptr<remote, util::invocable_constant<expire>>;
		using remote_handle::remote_handle;

		auto deref(const js::iv8::isolate_lock& lock) -> v8::Local<Type>;

		static auto make_shared(remote_handle_lock& lock, v8::Local<Type> handle) -> std::shared_ptr<remote>;
		static auto make_unique(remote_handle_lock& lock, v8::Local<Type> handle) -> unique_remote;

	private:
		// Deleter for the type-erased `expired_remote_type` `unique_ptr`
		static auto deleter(remote_handle* self) -> void;
		// "Deleter" for client smart pointers. Actually it wraps the handle in `expired_remote_type`
		// and sends it along to the delegated reset function.
		static auto expire(remote* self) -> void;
		static auto make(remote_handle_lock& lock, v8::Local<Type> handle) -> std::unique_ptr<remote>;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = remote<Type>::unique_remote;

export template <class Type>
auto make_shared_remote(remote_handle_lock& lock, v8::Local<Type> handle) -> shared_remote<Type> {
	return remote<Type>::make_shared(lock, handle);
}

// ---

template <class Type>
auto remote<Type>::deref(const js::iv8::isolate_lock& lock) -> v8::Local<Type> {
	// `handle.As<Type>()` doesn't work because types like `UnboundScript` don't provide a `Cast`
	// function.
	return std::bit_cast<v8::Local<Type>>(this->remote_handle::deref(lock));
}

template <class Type>
auto remote<Type>::deleter(remote_handle* self) -> void {
	delete static_cast<remote*>(self);
}

template <class Type>
auto remote<Type>::expire(remote* self) -> void {
	expired_remote_type ptr{self, deleter};
	remote_handle::expire(std::move(ptr));
}

template <class Type>
auto remote<Type>::make(remote_handle_lock& lock, v8::Local<Type> handle) -> std::unique_ptr<remote> {
	auto data_handle = std::bit_cast<v8::Local<v8::Data>>(handle);
	auto expire = lock.remote_expiration_task();
	auto* isolate = v8::Isolate::GetCurrent();
	return std::make_unique<remote>(v8::Global<v8::Data>{isolate, data_handle}, std::move(expire));
}

template <class Type>
auto remote<Type>::make_shared(remote_handle_lock& lock, v8::Local<Type> handle) -> std::shared_ptr<remote> {
	auto result = std::shared_ptr<remote>{make(lock, handle).release(), expire};
	lock.accept_remote_handle(*result);
	return result;
}

template <class Type>
auto remote<Type>::make_unique(remote_handle_lock& lock, v8::Local<Type> handle) -> unique_remote {
	auto result = unique_remote{make(lock, handle).release()};
	lock.accept_remote_handle(*result);
	return result;
}

} // namespace isolated_v8
