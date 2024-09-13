module;
#include <js_native_api.h>
#include <stdexcept>
#include <utility>
// Fix visibility bug on llvm 18
// /__w/ivm/ivm/packages/nodejs/include/external.cc:24:22: error: declaration of 'External' must be imported from module 'v8' before it is required
//   24 |         auto external = v8::External::New(env.isolate(), collected.get());
// /__w/ivm/ivm/packages/third_party/v8/external.cc:5:18: note: declaration here is not visible
//     5 | export using v8::External;
#include <v8-external.h>
module ivm.node:external;
import :environment;
import :utility;
import :visit;
import ivm.utility;
import ivm.value;
import ivm.v8;
import napi;

namespace ivm {

auto collection_group_finalizer(napi_env /*env*/, void* finalize_data, void* finalize_hint) {
	static_cast<collection_group*>(finalize_hint)->collect(finalize_data);
}

export template <class Type>
auto make_collected_external(environment& env, auto&&... args) -> Napi::Value {
	// We manually create a `v8::External` to avoid napi's leaky `ExternalWrapper` class. Also avoids
	// an additional dereference on the accessing side.
	auto collected = env.collection().make_ptr<iv8::external_reference<Type>>(std::forward<decltype(args)>(args)...);
	auto external = v8::External::New(env.isolate(), collected.get());
	auto as_napi_value = v8_to_napi(external);
	// Now we make a weak reference via napi (which also leaks) which finalizes the underlying object.
	if (napi_add_finalizer(env.napi_env(), as_napi_value, collected.get(), collection_group_finalizer, &env.collection(), nullptr) == napi_ok) {
		collected.release();
	} else {
		throw std::runtime_error("Failed to create external reference");
	}
	return {env.napi_env(), as_napi_value};
}

} // namespace ivm
