#include <node.h>
#include "isolate/util.h"
#include "isolate/environment.h"
#include "isolate/platform_delegate.h"
#include "context_handle.h"
#include "external_copy_handle.h"
#include "isolate_handle.h"
#include "lib_handle.h"
#include "native_module_handle.h"
#include "reference_handle.h"
#include "script_handle.h"

#include <memory>

using namespace v8;

namespace ivm {

/**
 * The whole library is transferable so you can Inception the library into your isolates.
 */
class LibraryHandle : public TransferableHandle {
	private:
		class LibraryHandleTransferable : public Transferable {
			public:
				Local<Value> TransferIn() final {
					return LibraryHandle::Get();
				}
		};

	public:
		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"isolated_vm", nullptr,
				"Context", ClassHandle::GetFunctionTemplate<ContextHandle>(),
				"ExternalCopy", ClassHandle::GetFunctionTemplate<ExternalCopyHandle>(),
				"Isolate", ClassHandle::GetFunctionTemplate<IsolateHandle>(),
				"NativeModule", ClassHandle::GetFunctionTemplate<NativeModuleHandle>(),
				"Reference", ClassHandle::GetFunctionTemplate<ReferenceHandle>(),
				"Script", ClassHandle::GetFunctionTemplate<ScriptHandle>()
			));
		}

		std::unique_ptr<Transferable> TransferOut() final {
			return std::make_unique<LibraryHandleTransferable>();
		}

		static Local<Object> Get() {
			Local<Object> library = ClassHandle::NewInstance<LibraryHandle>().As<Object>();
			library->Set(v8_symbol("lib"), ClassHandle::NewInstance<LibHandle>());
			return library;
		}
};

// Module entry point
std::shared_ptr<IsolateHolder> root_isolate;
extern "C"
void init(Local<Object> target) {

	// These flags will override limits set through code. Since the main node isolate is already
	// created we can reset these so they won't affect the isolates we make.
	int argc = 4;
	const char* flags[] = {
		"--max-semi-space-size", "0",
		"--max-old-space-size", "0"
	};
	V8::SetFlagsFromCommandLine(&argc, const_cast<char**>(flags), false);

	Isolate* isolate = Isolate::GetCurrent();
	root_isolate = IsolateEnvironment::New(isolate, isolate->GetCurrentContext());
	target->Set(v8_symbol("ivm"), LibraryHandle::Get());
	PlatformDelegate::InitializeDelegate();
}

} // namespace ivm

NODE_MODULE(isolated_vm, ivm::init)
