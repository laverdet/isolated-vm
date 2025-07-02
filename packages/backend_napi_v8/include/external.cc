module;
#include <js_native_api.h>
#include <memory>
#include <utility>
export module backend_napi_v8.external;
import backend_napi_v8.environment;
import backend_napi_v8.utility;
import isolated_js;
import ivm.utility;
import napi_js;
import napi_js.finalizer;
import nodejs;
import v8_js;
import v8;

namespace backend_napi_v8 {

export template <class Type>
auto make_external(environment& env, auto&&... args) -> napi_value {
	// We manually create a `v8::External` to avoid napi's leaky `ExternalWrapper` class. Also avoids
	// an additional dereference on the accessing side.
	using external_type = js::iv8::external_reference<Type>;
	auto ptr = std::make_unique<external_type>(std::forward<decltype(args)>(args)...);
	auto external = v8::External::New(env.isolate(), ptr.get());
	auto as_napi_value = js::napi::from_v8(external);
	// Now we make a weak reference via napi which finalizes the underlying object.
	js::napi::apply_finalizer(std::move(ptr), [ & ](external_type* data, napi_finalize finalize, void* hint) {
		js::napi::invoke0(napi_add_finalizer, napi_env{env}, as_napi_value, data, finalize, hint, nullptr);
	});
	return as_napi_value;
}

} // namespace backend_napi_v8
