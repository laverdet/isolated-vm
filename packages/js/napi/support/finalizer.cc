module;
#include <concepts>
#include <memory>
export module napi_js:finalizer;
import nodejs;

namespace js::napi {

// Finalizer for basic unique_ptr
export template <class Type, class Deleter>
auto apply_finalizer(std::unique_ptr<Type, Deleter> ptr, std::invocable<Type*, napi_finalize, void*> auto accept) -> decltype(auto)
	requires std::is_empty_v<Deleter> {
	const napi_finalize finalize = [](napi_env /*env*/, void* data, void* /*hint*/) -> void {
		Deleter deleter{};
		deleter(static_cast<Type*>(data));
	};
	void* const hint = nullptr;
	if constexpr (std::is_void_v<std::invoke_result_t<decltype(accept), Type*, napi_finalize, void*>>) {
		accept(ptr.get(), finalize, hint);
		ptr.release();
	} else {
		auto result = accept(ptr.get(), finalize, hint);
		ptr.release();
		return result;
	}
}

} // namespace js::napi
