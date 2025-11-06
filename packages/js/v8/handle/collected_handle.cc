module;
#include <functional>
#include <utility>
export module v8_js:collected_handle;
import :lock;
import ivm.utility;
import v8;

namespace js::iv8 {

// Lock interface required to create `collected_handle_lock<V, T>` handles.
export class collected_handle_lock : util::non_copyable {
	public:
		explicit collected_handle_lock(isolate_lock_witness lock, util::autorelease_pool& pool) :
				pool_{pool},
				isolate_{lock.isolate()} {}
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }
		[[nodiscard]] auto pool() const -> util::autorelease_pool& { return pool_; }

	private:
		std::reference_wrapper<util::autorelease_pool> pool_;
		v8::Isolate* isolate_;
};

// Weak referenced value which cleans up resources when collected by V8. It also makes use of
// `util::autorelease_pool` to cleanup orphaned resources when the host disposes itself.
export template <class Value, class Type>
class collected_handle
		: util::non_moveable,
			public util::pointer_facade {
	public:
		using unique_ptr = util::autorelease_pool::unique_ptr<collected_handle>;
		explicit collected_handle(util::autorelease_pool& pool, auto&&... args) :
				pool_{pool},
				value_{std::forward<decltype(args)>(args)...} {}

		auto operator*() -> Type& { return value_; }
		static auto make(const collected_handle_lock& lock, auto&&... args) -> unique_ptr
			requires std::constructible_from<Type, decltype(args)...>;
		static auto reset(const collected_handle_lock& lock, unique_ptr handle, v8::Local<Value> value) -> void;

	private:
		std::reference_wrapper<util::autorelease_pool> pool_;
		v8::Global<Value> global_;
		Type value_;
};

// Untagged v8-managed host object
export template <class Type>
using collected_external_of = collected_handle<v8::External, Type>;

// ---

template <class Value, class Type>
auto collected_handle<Value, Type>::make(const collected_handle_lock& lock, auto&&... args) -> unique_ptr
	requires std::constructible_from<Type, decltype(args)...> {
	return lock.pool().make_unique<collected_handle>(lock.pool(), std::forward<decltype(args)>(args)...);
}

template <class Value, class Type>
auto collected_handle<Value, Type>::reset(const collected_handle_lock& lock, unique_ptr handle, v8::Local<Value> value) -> void {
	util::move_pointer_operation(std::move(handle), [ & ](collected_handle* ptr, unique_ptr unique) -> void {
		ptr->global_.Reset(lock.isolate(), value);
		ptr->global_.SetWeak(
			unique.release(),
			[](const v8::WeakCallbackInfo<collected_handle>& data) {
				auto* ptr = data.GetParameter();
				auto unique = ptr->pool_.get().reacquire(ptr);
				unique->global_.ClearWeak();
			},
			v8::WeakCallbackType::kParameter
		);
	});
}

export template <class Type>
auto make_collected_external(const collected_handle_lock& lock, auto&&... args) -> v8::Local<v8::External> {
	auto unique = collected_external_of<Type>::make(lock, std::forward<decltype(args)>(args)...);
	auto value = v8::External::New(lock.isolate(), unique.get());
	collected_handle<v8::External, Type>::reset(lock, std::move(unique), value);
	return value;
}

export template <class Type>
auto unwrap_collected_external(v8::Local<v8::External> external) -> Type& {
	using handle_type = collected_external_of<Type>;
	return **static_cast<handle_type*>(external->Value());
}

} // namespace js::iv8
