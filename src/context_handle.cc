#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"
#include "module/evaluation.h"
#include "context_handle.h"
#include "reference_handle.h"
#include "transferable.h"

using namespace v8;

namespace ivm {
namespace {

/**
 * Instances of this turn into a ContextHandle when they are transferred in
 */
class ContextHandleTransferable : public Transferable {
	public:
		ContextHandleTransferable(RemoteHandle<Context> context, RemoteHandle<Value> global) :
			context{std::move(context)}, global{std::move(global)} {}

		auto TransferIn() -> Local<Value> final {
			return ClassHandle::NewInstance<ContextHandle>(std::move(context), std::move(global));
		}

	private:
		RemoteHandle<v8::Context> context;
		RemoteHandle<v8::Value> global;
};

} // anonymous namespace

/**
 * ContextHandle implementation
 */
ContextHandle::ContextHandle(RemoteHandle<Context> context, RemoteHandle<Value> global) :
	context{std::move(context)}, global{std::move(global)} {}

Local<FunctionTemplate> ContextHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Context", nullptr,
		"eval", MemberFunction<decltype(&ContextHandle::Eval<1>), &ContextHandle::Eval<1>>{},
		"evalIgnored", MemberFunction<decltype(&ContextHandle::Eval<2>), &ContextHandle::Eval<2>>{},
		"evalSync", MemberFunction<decltype(&ContextHandle::Eval<0>), &ContextHandle::Eval<0>>{},
		"global", MemberAccessor<decltype(&ContextHandle::GlobalGetter), &ContextHandle::GlobalGetter>{},
		"release", MemberFunction<decltype(&ContextHandle::Release), &ContextHandle::Release>{}
	));
}

auto ContextHandle::TransferOut() -> std::unique_ptr<Transferable> {
	return std::make_unique<ContextHandleTransferable>(context, global);
}

auto ContextHandle::GetContext() const -> RemoteHandle<v8::Context> {
	if (!context) {
		throw RuntimeGenericError("Context is released");
	}
	return context;
}

auto ContextHandle::GlobalGetter() -> Local<Value> {
	Isolate* isolate = Isolate::GetCurrent();
	if (!context) {
		return Undefined(isolate);
	}
	Local<Object> ref;
	if (global_reference) {
		ref = Deref(global_reference);
	} else {
		ref = ClassHandle::NewInstance<ReferenceHandle>(global.GetSharedIsolateHolder(), global, context, ReferenceHandle::TypeOf::Object);
		global_reference = RemoteHandle<v8::Object>(ref);
	}
	Unmaybe(This()->CreateDataProperty(isolate->GetCurrentContext(), v8_string("global"), ref));
	return ref;
}

auto ContextHandle::Release() -> Local<Value> {
	return Boolean::New(Isolate::GetCurrent(), [&]() {
		if (context) {
			context = {};
			global = {};
			if (global_reference) {
				ClassHandle::Unwrap<ReferenceHandle>(Deref(global_reference))->Release();
				global_reference = {};
			}
			return true;
		} else {
			return false;
		}
	}());
}

/*
 * Compiles and immediately executes a given script
 */
class EvalRunner : public ThreePhaseTask {
	public:
		explicit EvalRunner(
			RemoteHandle<Context> context,
			Local<String> code,
			MaybeLocal<Object> maybe_options
		) :
				script_origin_holder{maybe_options},
				transfer_options{maybe_options},
				context{std::move(context)},
				code_string{std::make_unique<ExternalCopyString>(code)} {
			if (!this->context) {
				throw RuntimeGenericError("Context is released");
			}
			timeout_ms = ReadOption<int32_t>(maybe_options, "timeout", timeout_ms);
		}

		void Phase2() final {
			// Load script in and compile
			auto isolate = IsolateEnvironment::GetCurrent();
			auto context = this->context.Deref();
			Context::Scope context_scope{context};
			IsolateEnvironment::HeapCheck heap_check{*isolate, true};
			ScriptOrigin script_origin = ScriptOrigin{script_origin_holder};
			ScriptCompiler::Source source{code_string->CopyIntoCheckHeap().As<String>(), script_origin};
			auto script = RunWithAnnotatedErrors([&]() {
				return Unmaybe(ScriptCompiler::Compile(context, &source));
			});
			// Execute script and transfer out
			Local<Value> script_result = RunWithTimeout(timeout_ms, [&]() {
				return script->Run(context);
			});
			result = Transferable::OptionalTransferOut(script_result, transfer_options);
			heap_check.Epilogue();
		}

		auto Phase3() -> Local<Value> final {
			if (result) {
				return result->TransferIn();
			} else {
				return Undefined(Isolate::GetCurrent()).As<Value>();
			}
		}

	private:
		ScriptOriginHolder script_origin_holder;
		Transferable::Options transfer_options;
		RemoteHandle<Context> context;
		std::unique_ptr<ExternalCopyString> code_string;
		std::unique_ptr<Transferable> result;
		int32_t timeout_ms = 0;
};

template <int Async>
auto ContextHandle::Eval(Local<String> code, MaybeLocal<Object> maybe_options) -> Local<Value> {
	return ThreePhaseTask::Run<Async, EvalRunner>(*context.GetIsolateHolder(), context, code, maybe_options);
}

} // namespace ivm
