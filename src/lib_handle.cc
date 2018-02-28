#include "lib_handle.h"
#include <uv.h>

using namespace v8;
using std::unique_ptr;

namespace ivm {

/**
 * Stateless transferable interface
 */
Local<Value> LibHandle::LibTransferable::TransferIn() {
	return ClassHandle::NewInstance<LibHandle>();
}

/**
 * ivm.lib API container
 */
Local<FunctionTemplate> LibHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Lib", nullptr,
		"hrtime", Parameterize<decltype(&LibHandle::Hrtime), &LibHandle::Hrtime>()
	));
}

unique_ptr<Transferable> LibHandle::TransferOut() {
	return std::make_unique<LibTransferable>();
}

Local<Value> LibHandle::Hrtime(MaybeLocal<Array> maybe_diff) {
	Isolate* isolate = Isolate::GetCurrent();
	Local<Context> context = isolate->GetCurrentContext();
	uint64_t time = uv_hrtime();
	constexpr uint64_t kNanos = (uint64_t)1e9;
	Local<Array> diff;
	if (maybe_diff.ToLocal(&diff)) {
		if (diff->Length() != 2) {
			throw js_type_error("hrtime diff must be 2-length array");
		}
		uint64_t time_diff = Unmaybe(diff->Get(context, 0)).As<Uint32>()->Value() * kNanos + Unmaybe(diff->Get(context, 1)).As<Uint32>()->Value();
		time -= time_diff;
	}
	Local<Array> ret = Array::New(isolate, 2);
	Unmaybe(ret->Set(context, 0, Uint32::New(isolate, (uint32_t)(time / kNanos))));
	Unmaybe(ret->Set(context, 1, Uint32::New(isolate, (uint32_t)(time - (time / kNanos) * kNanos))));
	return ret;
}

} // namespace ivm
