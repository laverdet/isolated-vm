module;
#include <utility>
export module isolated_v8.collected_handle;
import isolated_v8.lock;
import v8;
import ivm.utility;

namespace isolated_v8 {

export template <class Value, class Type>
class collected_handle
		: util::non_moveable,
			public util::pointer_facade<collected_handle<Value, Type>> {
	public:
		using unique_ptr = util::autorelease_pool::unique_ptr<collected_handle>;
		explicit collected_handle(util::autorelease_pool& pool, auto&&... args) :
				pool_{&pool},
				value_{std::forward<decltype(args)>(args)...} {}

		auto operator*() -> Type& { return value_; }
		static auto make(util::autorelease_pool& pool, auto&&... args) -> unique_ptr;
		static auto reset(const isolate_lock& lock, unique_ptr handle, v8::Local<Value> value) -> void;

	private:
		util::autorelease_pool* pool_;
		v8::Global<Value> global_;
		Type value_;
};

template <class Value, class Type>
auto collected_handle<Value, Type>::make(util::autorelease_pool& pool, auto&&... args) -> unique_ptr {
	return pool.make_unique<collected_handle<Value, Type>>(pool, std::forward<decltype(args)>(args)...);
}

template <class Value, class Type>
auto collected_handle<Value, Type>::reset(const isolate_lock& lock, unique_ptr handle, v8::Local<Value> value) -> void {
	util::move_pointer_operation(std::move(handle), [ & ](auto* ptr, auto unique) {
		ptr->global_.Reset(lock.isolate(), value);
		ptr->global_.SetWeak(
			unique.release(),
			[](const v8::WeakCallbackInfo<collected_handle>& data) {
				collected_handle* ptr = data.GetParameter();
				ptr->global_.ClearWeak();
				ptr->pool_->reacquire(ptr);
			},
			v8::WeakCallbackType::kParameter
		);
	});
}

export template <class Type>
auto make_collected_external(const isolate_lock& lock, util::autorelease_pool& pool, auto&&... args) -> v8::Local<v8::External> {
	auto handle = collected_handle<v8::External, Type>::make(pool, std::forward<decltype(args)>(args)...);
	return util::move_pointer_operation(std::move(handle), [ & ](auto* ptr, auto unique) {
		auto value = v8::External::New(lock.isolate(), ptr);
		collected_handle<v8::External, Type>::reset(lock, std::move(unique), value);
		return value;
	});
}

export template <class Type>
auto unwrap_collected_external(v8::Local<v8::External> external) -> Type& {
	using handle_type = collected_handle<v8::External, Type>;
	auto& handle = *static_cast<handle_type*>(external->Value());
	return *handle;
}

} // namespace isolated_v8
