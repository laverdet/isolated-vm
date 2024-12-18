module;
#include <js_native_api.h>
#include <stdexcept>
#include <utility>
module ivm.node:external;
import :environment;
import :utility;
import ivm.iv8;
import ivm.napi;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm {

auto collection_group_finalizer(napi_env /*env*/, void* finalize_data, void* finalize_hint) {
	static_cast<util::collection_group*>(finalize_hint)->collect(finalize_data);
}

export template <class Type>
auto make_collected_external(environment& env, auto&&... args) -> napi_value {
	// We manually create a `v8::External` to avoid napi's leaky `ExternalWrapper` class. Also avoids
	// an additional dereference on the accessing side.
	auto collected = env.collection().make_ptr<iv8::external_reference<Type>>(std::forward<decltype(args)>(args)...);
	auto external = v8::External::New(env.isolate(), collected.get());
	auto as_napi_value = ivm::napi::from_v8(external);
	// Now we make a weak reference via napi (which also leaks) which finalizes the underlying object.
	if (napi_add_finalizer(env.nenv(), as_napi_value, collected.get(), collection_group_finalizer, &env.collection(), nullptr) == napi_ok) {
		collected.release();
	} else {
		throw std::runtime_error("Failed to create external reference");
	}
	return as_napi_value;
}

} // namespace ivm
