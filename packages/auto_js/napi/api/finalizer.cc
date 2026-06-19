export module napi_js:api.finalizer;
import nodejs;
import std;
import util;

namespace js::napi {

// Generic finalizer for basic `std::unique_ptr<T>`
template <class Env, class Type, class Deleter>
auto make_finalizer(std::type_identity<Env> /*type*/, std::unique_ptr<Type, Deleter> unique) {
	if constexpr (std::is_empty_v<Deleter>) {
		// Finalizer uses a default-constructed deleter
		auto finalize = [](Env /*env*/, void* data, void* /*hint*/) -> void {
			Deleter deleter{};
			deleter(static_cast<Type*>(data));
		};
		return std::tuple{finalize, nullptr, std::move(unique)};
	} else {
		static_assert(std::is_trivially_destructible_v<Deleter>);
		static_assert(sizeof(Deleter) == sizeof(void*));
		// Hint is the deleter object
		auto finalize = [](Env /*env*/, void* data, void* hint) -> void {
			std::bit_cast<Deleter>(hint)(static_cast<Type*>(data));
		};
		return std::tuple{finalize, std::bit_cast<void*>(unique.get_deleter()), std::move(unique)};
	}
}

// Generic finalizer for `std::shared_ptr<T>`
template <class Env, class Type>
auto make_finalizer(std::type_identity<Env> /*type*/, std::shared_ptr<Type> shared) {
	// `shared_ptr<T>*` is passed as the finalizer hint
	using shared_ptr_type = std::shared_ptr<Type>;
	auto* ptr = shared.get();
	auto ptr_ptr = std::make_unique<shared_ptr_type>(std::move(shared));
	auto finalize = [](Env /*env*/, void* /*data*/, void* hint) -> void {
		delete static_cast<shared_ptr_type*>(hint);
	};
	return std::tuple{finalize, ptr_ptr.get(), std::move(ptr_ptr)};
}

// Apply a finalizer to the given smart pointer type
template <class Pointer, class Apply>
auto apply_finalizer(Pointer smart_ptr, Apply apply) -> decltype(auto) {
	using element_type = Pointer::element_type;
	constexpr auto napi_env_type = []() consteval -> auto {
		if constexpr (std::invocable<Apply, element_type*, node_api_basic_finalize, void*>) {
			return type<node_api_basic_env>;
		} else {
			static_assert(std::invocable<Apply, element_type*, napi_finalize, void*>);
			return type<napi_env>;
		}
	}();
	auto* pointer = smart_ptr.get();
	auto [ finalize, hint, releasable ] = make_finalizer(napi_env_type, std::move(smart_ptr));
	auto result = util::regular_return{[ & ] -> decltype(auto) {
		return apply(pointer, finalize, hint);
	}}();
	releasable.release();
	return *std::move(result);
}

} // namespace js::napi
