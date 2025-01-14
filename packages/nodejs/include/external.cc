module;
#include <js_native_api.h>
#include <memory>
#include <utility>
module ivm.node:external;
import :environment;
import :utility;
import ivm.iv8;
import ivm.js;
import ivm.napi;
import ivm.utility;
import napi;
import v8;

namespace ivm {

template <class Type>
auto finalizer(napi_env /*env*/, void* finalize_data, void* /*finalize_hint*/) {
	delete static_cast<Type*>(finalize_data);
}

export template <class Type>
auto make_external(environment& env, auto&&... args) -> napi_value {
	// We manually create a `v8::External` to avoid napi's leaky `ExternalWrapper` class. Also avoids
	// an additional dereference on the accessing side.
	using external_type = js::iv8::external_reference<Type>;
	auto ptr = std::make_unique<external_type>(std::forward<decltype(args)>(args)...);
	auto external = v8::External::New(env.isolate(), ptr.get());
	auto as_napi_value = js::napi::from_v8(external);
	// Now we make a weak reference via napi (which also leaks) which finalizes the underlying object.
	js::napi::invoke(napi_add_finalizer, env.nenv(), as_napi_value, ptr.get(), finalizer<external_type>, nullptr);
	ptr.release();
	return as_napi_value;
}

} // namespace ivm
