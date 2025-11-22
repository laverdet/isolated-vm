module;
#include <concepts>
#include <memory>
#include <type_traits>
export module napi_js:api.finalizer;
import nodejs;
import util;

namespace js::napi {

// Finalizer for basic unique_ptr
export template <class Type, class Deleter>
auto apply_finalizer(std::unique_ptr<Type, Deleter> unique, std::invocable<Type*, napi_finalize, void*> auto accept) -> decltype(auto)
	requires std::is_empty_v<Deleter> {
	// Finalizer uses a default-constructed deleter
	const napi_finalize finalize = [](napi_env /*env*/, void* data, void* /*hint*/) -> void {
		Deleter deleter{};
		deleter(static_cast<Type*>(data));
	};
	auto result = util::regular_return{[ & ]() -> decltype(auto) {
		return accept(unique.get(), finalize, nullptr);
	}}();
	unique.release();
	return *std::move(result);
}

// Finalizer for shared_ptr
export template <class Type>
auto apply_finalizer(std::shared_ptr<Type> shared, std::invocable<Type*, napi_finalize, void*> auto accept) -> decltype(auto) {
	// `shared_ptr<T>*` is passed as the finalizer hint
	using shared_ptr_type = std::shared_ptr<Type>;
	auto* ptr = shared.get();
	auto ptr_ptr = std::make_unique<shared_ptr_type>(std::move(shared));
	const napi_finalize finalize = [](napi_env /*env*/, void* /*data*/, void* hint) -> void {
		delete static_cast<shared_ptr_type*>(hint);
	};
	auto result = util::regular_return{[ & ]() -> decltype(auto) {
		return accept(ptr, finalize, ptr_ptr.get());
	}}();
	ptr_ptr.release();
	return *std::move(result);
}

} // namespace js::napi
