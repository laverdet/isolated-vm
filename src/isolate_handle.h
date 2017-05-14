#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "script_handle.h"

#include <memory>

namespace ivm {

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

class IsolateHandle : public ClassHandle {
	private:
		shared_ptr<ShareableIsolate> isolate;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass(
			 "Isolate", New, 1,
				"compileScriptSync", Method<IsolateHandle, &IsolateHandle::CompileScriptSync>, 1,
				"createContextSync", Method<IsolateHandle, &IsolateHandle::CreateContextSync>, 0
			);
		}

		IsolateHandle(shared_ptr<ShareableIsolate>& isolate) : isolate(isolate) {
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			if (!args.IsConstructCall()) {
				THROW(Exception::TypeError, "Class constructor Isolate cannot be invoked without 'new'");
			}
			Isolate::CreateParams create_params;
			create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
			auto isolate = std::make_shared<ShareableIsolate>(create_params);
			Transfer(std::make_unique<IsolateHandle>(isolate), args.This());
			args.GetReturnValue().Set(args.This());
		}

		void CreateContextSync(const FunctionCallbackInfo<Value>& args) {
			shared_ptr<ShareablePersistent<Context>> context_ptr;
			shared_ptr<ShareablePersistent<Value>> global_ptr;
			ShareableIsolate::Locker(*isolate, [ this, &context_ptr, &global_ptr ]() {
				Local<Context> context = Context::New(*isolate);
				context_ptr = std::make_shared<ShareablePersistent<Context>>(context);
				global_ptr = std::make_shared<ShareablePersistent<Value>>(context->Global());
				return 0;
			});
			args.GetReturnValue().Set(ClassHandle::NewInstance<ContextHandle>(context_ptr, global_ptr));
		}

		void CompileScriptSync(const FunctionCallbackInfo<Value>& args) {
			if (args.Length() < 1) {
				THROW(Exception::TypeError, "compileScriptSync expects 1 parameter");
			}
			TryCatch try_catch(Isolate::GetCurrent());
			String::Utf8Value code(Local<String>::Cast(args[0]));
			if (try_catch.HasCaught()) {
				try_catch.ReThrow();
				return;
			}
			const char* code_c = *code;
			shared_ptr<ShareablePersistent<UnboundScript>> script_ptr = ShareableIsolate::Locker(*isolate, [ this, code_c ]() -> shared_ptr<ShareablePersistent<UnboundScript>> {
				Context::Scope context_scope(isolate->DefaultContext());

				Local<String> code(String::NewFromUtf8(*isolate, code_c, NewStringType::kNormal).ToLocalChecked());
				ScriptCompiler::Source source(code);
				MaybeLocal<UnboundScript> script = ScriptCompiler::CompileUnboundScript(*isolate, &source, ScriptCompiler::kNoCompileOptions);
				if (script.IsEmpty()) {
					return nullptr;
				} else {
					return std::make_shared<ShareablePersistent<UnboundScript>>(script.ToLocalChecked());
				}
			});
			if (try_catch.HasCaught()) {
				try_catch.ReThrow();
			} else {
				args.GetReturnValue().Set(ClassHandle::NewInstance<ScriptHandle>(script_ptr));
			}
		}
};

}
