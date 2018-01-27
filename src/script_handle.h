#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "transferable_handle.h"
#include "context_handle.h"
#include "external_copy.h"

#include <memory>

namespace ivm {
using namespace v8;
using std::shared_ptr;

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
		ScriptHandle(std::shared_ptr<ShareablePersistent<UnboundScript>> script) : script(script) {}

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

		/*
		 * Run this script in a given context
		 */
		template <bool async>
		Local<Value> Run(ContextHandle* context_handle, MaybeLocal<Object> maybe_options) {
			class Run : public ThreePhaseTask {
				private:
					// phase 2
					uint32_t timeout_ms { 0 };
					shared_ptr<ShareablePersistent<UnboundScript>> script;
					shared_ptr<ShareableContext> context;
					// phase 3
					std::unique_ptr<Transferable> result;

				public:
					Run(
						MaybeLocal<Object>& maybe_options,
						shared_ptr<ShareablePersistent<UnboundScript>> script,
						shared_ptr<ShareableContext> context
					) :
						script(script), context(context)
					{
						// Sanity check
						if (script->GetIsolate() != context->GetIsolate()) {
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
						Local<Context> context_local = context->Deref();
						Context::Scope context_scope(context_local);
						Local<Script> script_handle = script->Deref()->BindToCurrentContext();
						result = ExternalCopy::CopyIfPrimitive(
							RunWithTimeout(timeout_ms, script->GetIsolate(), [&script_handle, &context_local]() { return script_handle->Run(context_local); })
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
			return ThreePhaseTask::Run<async, Run>(script->GetIsolate(), maybe_options, script, context_handle->context);
		}

};

}
