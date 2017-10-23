#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "transferable_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "timer.h"

#include <memory>

namespace ivm {
using namespace v8;

class ScriptHandle : public TransferableHandle {
	private:
		std::shared_ptr<ShareablePersistent<UnboundScript>> script;

		class ScriptHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareablePersistent<UnboundScript>> script;
			public:
				ScriptHandleTransferable(shared_ptr<ShareablePersistent<UnboundScript>>& script) : script(script) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ScriptHandle>(script);
				}
		};

	public:
		ScriptHandle(std::shared_ptr<ShareablePersistent<UnboundScript>>& script) : script(script) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Script", nullptr, 0,
				"run", Parameterize<decltype(&ScriptHandle::Run<true>), &ScriptHandle::Run<true>>, 1,
				"runSync", Parameterize<decltype(&ScriptHandle::Run<false>), &ScriptHandle::Run<false>>, 1
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ScriptHandleTransferable>(script);
		}

		template <bool async>
		Local<Value> Run(ContextHandle* context_handle, MaybeLocal<Object> maybe_options) {
			return ThreePhaseRunner<async>(script->GetIsolate(), [this, &maybe_options]() {
				// Get run options
				Local<Object> options;
				uint32_t timeout = 0;
				if (maybe_options.ToLocal(&options)) {
					Local<Value> timeout_handle = Unmaybe(options->Get(Isolate::GetCurrent()->GetCurrentContext(), v8_string("timeout")));
					if (!timeout_handle->IsUndefined()) {
						if (!timeout_handle->IsUint32()) {
							throw js_type_error("`timeout` must be integer");
						}
						timeout = timeout_handle.As<Uint32>()->Value();
					}
				}
				return timeout;

			}, [this, context_handle](uint32_t timeout_ms) {
				// Enter script's context and run it
				Isolate* isolate = Isolate::GetCurrent();
				Local<Context> context = context_handle->context->Deref();
				Context::Scope context_scope(context);
				Local<Script> script = this->script->Deref()->BindToCurrentContext();
				bool did_timeout = false;
				shared_ptr<bool> did_finish = std::make_shared<bool>(false);
				MaybeLocal<Value> result;
				{
					unique_ptr<timer_t> timer_ptr;
					if (timeout_ms != 0) {
						timer_ptr = std::make_unique<timer_t>(timeout_ms, std::bind([&did_timeout, isolate](shared_ptr<bool> did_finish) {
							did_timeout = true;
							isolate->TerminateExecution();
							// FIXME(?): It seems that one call to TerminateExecution() doesn't kill the script if
							// there is a promise handler scheduled. This is unexpected behavior but I can't
							// reproduce it in vanilla v8 so the issue seems more complex. I'm punting on this for
							// now with a hack but will look again when nodejs pulls in a newer version of v8 with
							// more mature microtask support.
							//
							// This loop always terminates for me in 1 iteration but it goes up to 100 because the
							// only other option is terminating the application if an isolate has gone out of
							// control.
							for (int ii = 0; ii < 100; ++ii) {
								std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(2));
								if (*did_finish) {
									return;
								}
								isolate->TerminateExecution();
							}
							assert(false);
						}, did_finish));
					}
					result = script->Run(context);
					*did_finish = true;
				}
				if (did_timeout) {
					isolate->CancelTerminateExecution();
					throw js_generic_error("Script execution timed out.");
				}
				return shared_ptr<Transferable>(ExternalCopy::CopyIfPrimitive(Unmaybe(result)));
			}, [](shared_ptr<Transferable> result) {
				if (result.get() == nullptr) {
					return Undefined(Isolate::GetCurrent()).As<Value>();
				} else {
					return result->TransferIn();
				}
			});
		}
};

}
