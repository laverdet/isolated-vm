#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "script_handle.h"
#include "external_copy.h"

#include <stdlib.h>
#include <memory>
#include <map>
#include <cmath>
#include <algorithm>

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

		/**
		 * ArrayBuffer::Allocator that enforces memory limits. The v8 documentation specifically says
		 * that it's unsafe to call back into v8 from this class but I took a look at
		 * GetHeapStatistics() and I think it'll be ok.
		 */
		class LimitedAllocator : public ArrayBuffer::Allocator {
			private:
				size_t limit;
				size_t v8_heap;
				size_t my_heap;
				size_t next_check;

				void Update(const size_t length) {
					if (v8_heap + my_heap + length > next_check) {
						HeapStatistics heap_statistics;
						Isolate::GetCurrent()->GetHeapStatistics(&heap_statistics);
						v8_heap = heap_statistics.total_heap_size();
						next_check = v8_heap + my_heap + length + 1024 * 1024;
					}
				}

			public:
				LimitedAllocator(size_t limit) : limit(limit), v8_heap(1024 * 1024 * 4), my_heap(0), next_check(1024 * 1024) {}

				virtual void* Allocate(size_t length) {
					Update(length);
					if (v8_heap + my_heap + length > limit) {
						return nullptr;
					} else {
						my_heap += length;
						return calloc(length, 1);
					}
				}

				virtual void* AllocateUninitialized(size_t length) {
					Update(length);
					if (v8_heap + my_heap + length > limit) {
						return nullptr;
					} else {
						my_heap += length;
						return malloc(length);
					}
				}

				virtual void Free(void* data, size_t length) {
					my_heap -= length;
					next_check -= length;
					free(data);
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
				"createContextSync", Parameterize<decltype(&IsolateHandle::CreateContextSync), &IsolateHandle::CreateContextSync>, 0,
				"disposeSync", Parameterize<decltype(&IsolateHandle::DisposeSync), &IsolateHandle::DisposeSync>, 0
			));
		}

		/**
		 * Create a new Isolate. It all starts here!
		 */
		static unique_ptr<ClassHandle> New(MaybeLocal<Object> maybe_options) {
			Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
			Isolate::CreateParams create_params;
			unique_ptr<ArrayBuffer::Allocator> allocator_ptr(ArrayBuffer::Allocator::NewDefaultAllocator());
			create_params.array_buffer_allocator = allocator_ptr.get();

			// Parse options
			if (!maybe_options.IsEmpty()) {
				Local<Object> options = maybe_options.ToLocalChecked();

				// Set memory limits
				Local<Value> maybe_memory_limit = Unmaybe(options->Get(context, v8_symbol("memoryLimit")));
				if (maybe_memory_limit->IsNumber()) {
					size_t memory_limit = maybe_memory_limit.As<Number>()->Value();
					ResourceConstraints& rc = create_params.constraints;
					rc.set_max_semi_space_size(std::pow(2, std::min(sizeof(void*) >= 8 ? 4 : 3, (int)(memory_limit / 128))));
					rc.set_max_old_space_size(memory_limit);
					rc.set_max_executable_size(memory_limit * 0.75 + 0.5);
					allocator_ptr.reset(new LimitedAllocator(memory_limit));
					create_params.array_buffer_allocator = allocator_ptr.get();
				}
			}

			return std::make_unique<IsolateHandle>(std::make_shared<ShareableIsolate>(std::move(allocator_ptr), create_params));
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

		/**
		 * Dispose an isolate
		 */
		Local<Value> DisposeSync() {
			if (Locker::IsLocked(*isolate)) {
				throw js_generic_error("Cannot dispose entered isolate");
			}
			isolate->Dispose();
			return Undefined(Isolate::GetCurrent());
		}
};

}
