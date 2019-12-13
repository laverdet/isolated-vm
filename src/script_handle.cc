#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"
#include "context_handle.h"
#include "script_handle.h"

using namespace v8;

namespace ivm {

/**
 * ScriptHandle implementation
 */
ScriptHandle::ScriptHandle(RemoteHandle<UnboundScript> script) :
	script{std::move(script)} {}

auto ScriptHandle::Definition() -> Local<FunctionTemplate> {
	return Inherit<TransferableHandle>(MakeClass(
		"Script", nullptr,
		"release", MemberFunction<decltype(&ScriptHandle::Release), &ScriptHandle::Release>{},
		"run", MemberFunction<decltype(&ScriptHandle::Run<1>), &ScriptHandle::Run<1>>{},
		"runIgnored", MemberFunction<decltype(&ScriptHandle::Run<2>), &ScriptHandle::Run<2>>{},
		"runSync", MemberFunction<decltype(&ScriptHandle::Run<0>), &ScriptHandle::Run<0>>{}
	));
}

auto ScriptHandle::TransferOut() -> std::unique_ptr<Transferable> {
	return std::make_unique<ScriptHandleTransferable>(script);
}

auto ScriptHandle::Release() -> Local<Value> {
	script = {};
	return Undefined(Isolate::GetCurrent());
}

/*
 * Run this script in a given context
 */
struct RunRunner /* lol */ : public ThreePhaseTask {
	RunRunner(
		RemoteHandle<UnboundScript>& script,
		ContextHandle& context_handle,
		MaybeLocal<Object> maybe_options
	) : context{context_handle.GetContext()} {
		// Sanity check
		if (!script) {
			throw js_generic_error("Script has been released");
		}
		if (script.GetIsolateHolder() != context.GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}

		// Parse options
		Isolate* isolate = Isolate::GetCurrent();
		bool release = false;
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
		if (release) {
			this->script = std::move(script);
		} else {
			this->script = script;
		}
		transfer_options = Transferable::Options{maybe_options};
	}

	void Phase2() final {
		// Enter script's context and run it
		Local<Context> context_local = Deref(context);
		Context::Scope context_scope{context_local};
		Local<Script> script_handle = Deref(script)->BindToCurrentContext();
		Local<Value> script_result = RunWithTimeout(timeout_ms, [&script_handle, &context_local]() {
			return script_handle->Run(context_local);
		});
		result = Transferable::OptionalTransferOut(script_result, transfer_options);
	}

	auto Phase3() -> Local<Value> final {
		if (result) {
			return result->TransferIn();
		} else {
			return Undefined(Isolate::GetCurrent()).As<Value>();
		}
	}

	RemoteHandle<UnboundScript> script;
	RemoteHandle<Context> context;
	Transferable::Options transfer_options;
	std::unique_ptr<Transferable> result;
	uint32_t timeout_ms = 0;
};
template <int async>
auto ScriptHandle::Run(ContextHandle& context_handle, MaybeLocal<Object> maybe_options) -> Local<Value> {
	return ThreePhaseTask::Run<async, RunRunner>(*script.GetIsolateHolder(), script, context_handle, maybe_options);
}

/**
 * ScriptHandleTransferable implementation
 */
ScriptHandle::ScriptHandleTransferable::ScriptHandleTransferable(RemoteHandle<UnboundScript> script) :
	script{std::move(script)} {}

auto ScriptHandle::ScriptHandleTransferable::TransferIn() -> Local<Value> {
	return ClassHandle::NewInstance<ScriptHandle>(script);
};

} // namespace ivm
