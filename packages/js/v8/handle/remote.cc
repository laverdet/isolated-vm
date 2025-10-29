module;
#include <memory>
export module v8_js:remote;
export import :remote_handle;
import ivm.utility;
import v8;

namespace js::iv8 {

// Reference to a persistent in v8. It should always be created with `make_shared_remote` or
// `make_unique_remote`. When the smart pointer is released the handle schedules a task to reset
// itself in the isolate thread.
template <class Type, class Storage>
class remote {
	private:
		explicit remote(Storage storage);

	public:
		auto deref(isolate_lock_witness lock) const -> v8::Local<Type>;
		static auto make(const remote_handle_lock& lock, v8::Local<Type> local) -> remote;

	private:
		Storage storage_;
};

// Convenience helpers
export template <class Type>
using unique_remote = remote<Type, remote_handle::unique_type>;

export template <class Type>
using shared_remote = remote<Type, remote_handle::shared_type>;

export template <class Type>
auto make_unique_remote(const remote_handle_lock& lock, v8::Local<Type> handle) -> unique_remote<Type> {
	return unique_remote<Type>::make(lock, handle);
}

export template <class Type>
auto make_shared_remote(const remote_handle_lock& lock, v8::Local<Type> handle) -> shared_remote<Type> {
	return shared_remote<Type>::make(lock, handle);
}

// ---

template <class Type, class Storage>
remote<Type, Storage>::remote(Storage storage) :
		storage_{std::move(storage)} {}

template <class Type, class Storage>
auto remote<Type, Storage>::deref(isolate_lock_witness lock) const -> v8::Local<Type> {
	return storage_->deref(lock, type<Type>);
}

template <class Type, class Storage>
auto remote<Type, Storage>::make(const remote_handle_lock& lock, v8::Local<Type> local) -> remote {
	auto expire = remote_handle::expire{lock.expiry_scheduler()};
	auto handle = std::make_unique<remote_handle>(lock.witness(), local);
	auto handle_with_expire = remote_handle::unique_type{nullptr, std::move(expire)};
	handle_with_expire.reset(handle.release());
	lock.handle_list().insert(*handle_with_expire);
	return remote{Storage{std::move(handle_with_expire)}};
}

} // namespace js::iv8
