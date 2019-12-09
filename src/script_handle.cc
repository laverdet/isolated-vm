#include "script_handle.h"
#include "context_handle.h"
#include "transferable.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

using namespace v8;
using std::shared_ptr;

namespace ivm {

ScriptHandle::ScriptHandleTransferable::ScriptHandleTransferable(
	RemoteHandle<UnboundScript> script
) : script(std::move(script)) {}

Local<Value> ScriptHandle::ScriptHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ScriptHandle>(script);
};

ScriptHandle::ScriptHandle(
	RemoteHandle<UnboundScript> script
) : script(std::move(script)) {}

Local<FunctionTemplate> ScriptHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Script", nullptr,
		"release", Parameterize<decltype(&ScriptHandle::Release), &ScriptHandle::Release>(),
		"run", Parameterize<decltype(&ScriptHandle::Run<1>), &ScriptHandle::Run<1>>(),
		"runIgnored", Parameterize<decltype(&ScriptHandle::Run<2>), &ScriptHandle::Run<2>>(),
		"runSync", Parameterize<decltype(&ScriptHandle::Run<0>), &ScriptHandle::Run<0>>()
	));
}

std::unique_ptr<Transferable> ScriptHandle::TransferOut() {
	return std::make_unique<ScriptHandleTransferable>(script);
}

/*
 * Run this script in a given context
 */
struct RunRunner /* lol */ : public ThreePhaseTask {
	uint32_t timeout_ms = 0;
	RemoteHandle<UnboundScript> script;
	RemoteHandle<Context> context;
	std::unique_ptr<Transferable> result;

	RunRunner(
		RemoteHandle<UnboundScript> script,
		uint32_t timeout_ms,
		ContextHandle* context_handle
	) : timeout_ms(timeout_ms), script(std::move(script)), context(context_handle->context) {
		// Sanity check
		context_handle->CheckDisposed();
		if (this->script.GetIsolateHolder() != context_handle->context.GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
		// Enter script's context and run it
		Local<Context> context_local = Deref(context);
		Context::Scope context_scope(context_local);
		Local<Script> script_handle = Deref(script)->BindToCurrentContext();
		result = Transferable::OptionalTransferOut(
			RunWithTimeout(timeout_ms, [&script_handle, &context_local]() { return script_handle->Run(context_local); })
		);
	}

	Local<Value> Phase3() final {
		if (result) {
			return result->TransferIn();
		} else {
			return Undefined(Isolate::GetCurrent()).As<Value>();
		}
	}
};
template <int async>
Local<Value> ScriptHandle::Run(ContextHandle* context_handle, MaybeLocal<Object> maybe_options) {
	if (!script) {
		throw js_generic_error("Script has been released");
	}
	Isolate* isolate = Isolate::GetCurrent();
	bool release = false;
	uint32_t timeout_ms = 0;
	Local<Object> options;
	if (maybe_options.ToLocal(&options)) {
		release = IsOptionSet(isolate->GetCurrentContext(), options, "release");
		Local<Value> timeout_handle = Unmaybe(options->Get(isolate->GetCurrentContext(), v8_string("timeout")));
		if (!timeout_handle->IsUndefined()) {
			if (!timeout_handle->IsUint32()) {
				throw js_type_error("`timeout` must be integer");
			}
			timeout_ms = timeout_handle.As<Uint32>()->Value();
		}
	}
	RemoteHandle<UnboundScript> script_ref = script;
	if (release) {
		script = {};
	}
	return ThreePhaseTask::Run<async, RunRunner>(*script_ref.GetIsolateHolder(), std::move(script_ref), timeout_ms, context_handle);
}

Local<Value> ScriptHandle::Release() {
	script = {};
	return Undefined(Isolate::GetCurrent());
}

} // namespace ivm
