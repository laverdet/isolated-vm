#pragma once
#include <node.h>
#include "isolate/environment.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"
#include "transferable_handle.h"
#include "context_handle.h"
#include "external_copy.h"

#include <memory>

namespace ivm {
using namespace v8;
using std::shared_ptr;

class ScriptHandle : public TransferableHandle {
	private:
		std::shared_ptr<v8::Persistent<v8::UnboundScript>> script;
		std::shared_ptr<IsolateHolder> isolate;

		class ScriptHandleTransferable : public Transferable {
			private:
				std::shared_ptr<Persistent<UnboundScript>> script;
				std::shared_ptr<IsolateHolder> isolate;
			public:
				ScriptHandleTransferable(
					shared_ptr<Persistent<UnboundScript>> script,
					shared_ptr<IsolateHolder> isolate
				) : script(std::move(script)), isolate(std::move(isolate)) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ScriptHandle>(script, isolate);
				}
		};

	public:
		ScriptHandle(
			std::shared_ptr<v8::Persistent<v8::UnboundScript>> script,
			std::shared_ptr<IsolateHolder> isolate
		) : script(std::move(script)), isolate(std::move(isolate)) {}

		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
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
			return std::make_unique<ScriptHandleTransferable>(script, isolate);
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
					shared_ptr<Persistent<UnboundScript>> script;
					shared_ptr<Persistent<Context>> context;
					// phase 3
					std::unique_ptr<Transferable> result;

				public:
					Run(
						MaybeLocal<Object>& maybe_options,
						shared_ptr<Persistent<UnboundScript>> script,
						IsolateHolder* isolate,
						ContextHandle* context_handle
					) :
						script(script), context(context_handle->context)
					{
						// Sanity check
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
						Local<Script> script_handle = Local<UnboundScript>::New(Isolate::GetCurrent(), *script)->BindToCurrentContext();
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
			return ThreePhaseTask::Run<async, Run>(*isolate, maybe_options, script, isolate.get(), context_handle);
		}

};

}
