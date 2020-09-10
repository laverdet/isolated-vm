#include "isolate/node_wrapper.h"
#include "isolate/util.h"
#include "isolate/environment.h"
#include "isolate/node_wrapper.h"
#include "isolate/platform_delegate.h"
#include "lib/lockable.h"
#include "context_handle.h"
#include "external_copy_handle.h"
#include "isolate_handle.h"
#include "lib_handle.h"
#include "native_module_handle.h"
#include "reference_handle.h"
#include "script_handle.h"

#include <memory>
#include <mutex>

using namespace v8;

namespace ivm {

/**
 * The whole library is transferable so you can Inception the library into your isolates.
 */
class LibraryHandle : public TransferableHandle {
	private:
		class LibraryHandleTransferable : public Transferable {
			public:
				auto TransferIn() -> Local<Value> final {
					return LibraryHandle::Get();
				}
		};

	public:
		static auto Definition() -> Local<FunctionTemplate> {
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

		auto TransferOut() -> std::unique_ptr<Transferable> final {
			return std::make_unique<LibraryHandleTransferable>();
		}

		static auto Get() -> Local<Object> {
			Local<Object> library = ClassHandle::NewInstance<LibraryHandle>().As<Object>();
			Unmaybe(library->Set(Isolate::GetCurrent()->GetCurrentContext(), v8_symbol("lib"), ClassHandle::NewInstance<LibHandle>()));
			return library;
		}
};

// Module entry point
std::atomic<bool> did_global_init{false};
// TODO: This is here so that these dtors don't get called when the module is being torn down. They
// end up invoking a bunch of v8 functions which fail because nodejs already shut down the platform.
struct IsolateHolderAndJoin {
	std::shared_ptr<IsolateHolder> holder;
	std::shared_ptr<IsolateDisposeWait> dispose_wait;
};
auto default_isolates = new lockable_t<std::unordered_map<v8::Isolate*, IsolateHolderAndJoin>>;
extern "C"
void init(Local<Object> target) {
	// Create default isolate env
	Isolate* isolate = Isolate::GetCurrent();
	Local<Context> context = isolate->GetCurrentContext();
#if NODE_MODULE_VERSION < 72
	if (node::GetCurrentEventLoop(isolate) != uv_default_loop()) {
		isolate->ThrowException(v8_string("nodejs 12.0.0 or higher is required to use `isolated-vm` from within `worker_threads`"));
	}
#endif
	// Maybe this would happen if you include the module from `vm`?
	{
		auto isolates = default_isolates->write();
		assert(isolates->find(isolate) == isolates->end());
		auto holder = IsolateEnvironment::New(isolate, context);
		isolates->insert(std::make_pair(
			isolate,
			IsolateHolderAndJoin{holder, holder->GetIsolate()->GetDisposeWaitHandle()}
		));
	}
	Unmaybe(target->Set(context, v8_symbol("ivm"), LibraryHandle::Get()));

	node::AddEnvironmentCleanupHook(isolate, [](void* param) {
		auto* isolate = static_cast<v8::Isolate*>(param);
		auto it = default_isolates->read()->find(isolate);
		it->second.holder->Release();
		it->second.dispose_wait->Join();
		default_isolates->write()->erase(isolate); // `it` might have changed, don't use it
	}, isolate);


	if (!did_global_init.exchange(true)) {
		// These flags will override limits set through code. Since the main node isolate is already
		// created we can reset these so they won't affect the isolates we make.
		int argc = 4;
		const char* flags[] = {
			"--max-semi-space-size", "0",
			"--max-old-space-size", "0"
		};
		V8::SetFlagsFromCommandLine(&argc, const_cast<char**>(flags), false);

		PlatformDelegate::InitializeDelegate();
	}
}

} // namespace ivm

NODE_MODULE_CONTEXT_AWARE(isolated_vm, ivm::init) // NOLINT
