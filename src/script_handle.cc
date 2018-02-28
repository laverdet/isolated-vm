#include "script_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

using namespace v8;
using std::shared_ptr;

namespace ivm {

ScriptHandle::ScriptHandleTransferable::ScriptHandleTransferable(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<RemoteHandle<UnboundScript>> script
) : isolate(std::move(isolate)), script(std::move(script)) {}

Local<Value> ScriptHandle::ScriptHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ScriptHandle>(isolate, script);
};

ScriptHandle::ScriptHandle(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<RemoteHandle<UnboundScript>> script
) : isolate(std::move(isolate)), script(std::move(script)) {}

Local<FunctionTemplate> ScriptHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Script", nullptr,
		"run", Parameterize<decltype(&ScriptHandle::Run<1>), &ScriptHandle::Run<1>>(),
		"runIgnored", Parameterize<decltype(&ScriptHandle::Run<2>), &ScriptHandle::Run<2>>(),
		"runSync", Parameterize<decltype(&ScriptHandle::Run<0>), &ScriptHandle::Run<0>>()
	));
}

std::unique_ptr<Transferable> ScriptHandle::TransferOut() {
	return std::make_unique<ScriptHandleTransferable>(isolate, script);
}

/*
 * Run this script in a given context
 */
struct RunRunner /* lol */ : public ThreePhaseTask {
	uint32_t timeout_ms = 0;
	shared_ptr<RemoteHandle<UnboundScript>> script;
	shared_ptr<RemoteHandle<Context>> context;
	std::unique_ptr<Transferable> result;

	RunRunner(
		const MaybeLocal<Object>& maybe_options,
		IsolateHolder* isolate,
		shared_ptr<RemoteHandle<UnboundScript>> script,
		ContextHandle* context_handle
	) : script(std::move(script)), context(context_handle->context) {
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate != context_handle->isolate.get()) {
			throw js_generic_error("Invalid context");
		}

		// Get run options
		Local<Object> options;
		if (maybe_options.ToLocal(&options)) {
			Local<Value> timeout_handle = Unmaybe(options->Get(Isolate::GetCurrent()->GetCurrentContext(), v8_string("timeout")));
			if (!timeout_handle->IsUndefined()) {
				if (!timeout_handle->IsUint32()) {
					throw js_type_error("`timeout` must be integer");
				}
				timeout_ms = timeout_handle.As<Uint32>()->Value();
			}
		}
	}

	void Phase2() final {
		// Enter script's context and run it
		Local<Context> context_local = Deref(*context);
		Context::Scope context_scope(context_local);
		Local<Script> script_handle = Deref(*script)->BindToCurrentContext();
		result = ExternalCopy::CopyIfPrimitive(
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
	return ThreePhaseTask::Run<async, RunRunner>(*isolate, maybe_options, isolate.get(), script, context_handle);
}

} // namespace ivm
