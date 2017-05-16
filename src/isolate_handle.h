#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "script_handle.h"
#include "external_copy.h"

#include <memory>
#include <map>

namespace ivm {

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

/**
 * Reference to a v8 isolate
 */
class IsolateHandle : public TransferableHandle {
	private:
		shared_ptr<ShareableIsolate> isolate;

		/**
		 * Wrapper class created when you pass an IsolateHandle through to another isolate
		 */
		class IsolateHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareableIsolate> isolate;
			public:
				IsolateHandleTransferable(shared_ptr<ShareableIsolate>& isolate) : isolate(isolate) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<IsolateHandle>(isolate);
				}
		};

	public:
		IsolateHandle(shared_ptr<ShareableIsolate> isolate) : isolate(isolate) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
			 "Isolate", Parameterize<decltype(New), New>, 1,
				"compileScriptSync", Parameterize<decltype(&IsolateHandle::CompileScriptSync), &IsolateHandle::CompileScriptSync>, 1,
				"createContextSync", Parameterize<decltype(&IsolateHandle::CreateContextSync), &IsolateHandle::CreateContextSync>, 0
			));
		}

		static unique_ptr<ClassHandle> New() {
			Isolate::CreateParams create_params;
			create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
			return std::make_unique<IsolateHandle>(std::make_shared<ShareableIsolate>(create_params));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<IsolateHandleTransferable>(isolate);
		}

		/**
		 * Create a new v8::Context in this isolate and returns a ContextHandle
		 */
		Local<Value> CreateContextSync() {
			shared_ptr<ShareablePersistent<Context>> context_ptr;
			shared_ptr<ShareablePersistent<Value>> global_ptr;
			ShareableIsolate::Locker(*isolate, [ this, &context_ptr, &global_ptr ]() {
				Local<Context> context = Context::New(*isolate);
				context_ptr = std::make_shared<ShareablePersistent<Context>>(context);
				global_ptr = std::make_shared<ShareablePersistent<Value>>(context->Global());
			});
			return ClassHandle::NewInstance<ContextHandle>(context_ptr, global_ptr);
		}

		/**
		 * Compiles a script in this isolate and returns a ScriptHandle
		 */
		Local<Value> CompileScriptSync(Local<String> code_handle) {
			ExternalCopyString code_copy(code_handle);
			return ClassHandle::NewInstance<ScriptHandle>(ShareableIsolate::Locker(*isolate, [ this, &code_copy ]() {
				Context::Scope context_scope(isolate->DefaultContext());
				Local<String> code_inner = code_copy.CopyInto().As<String>();
				ScriptCompiler::Source source(code_inner);
				Local<UnboundScript> script = Unmaybe(ScriptCompiler::CompileUnboundScript(*isolate, &source, ScriptCompiler::kNoCompileOptions));
				return std::make_shared<ShareablePersistent<UnboundScript>>(script);
			}));
		}
};

}
