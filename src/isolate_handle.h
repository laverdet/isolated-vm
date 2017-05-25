#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "script_handle.h"
#include "external_copy.h"
#include "external_copy_handle.h"

#include <stdlib.h>
#include <memory>
#include <map>
#include <cmath>
#include <algorithm>

namespace ivm {

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;
Local<Value> CreateSnapshot(Local<Array> script_handles, MaybeLocal<String> warmup_handle);

/**
 * Parses script origin information from an option object and returns a non-v8 holder for the
 * information which can then be converted to a ScriptOrigin, perhaps in a different isolate from
 * the one it was read in.
 */
class ScriptOriginHolder {
	private:
		std::string filename;
		int columnOffset;
		int lineOffset;

	public:
		ScriptOriginHolder(MaybeLocal<Object> maybe_options) : filename("<isolated-vm>"), columnOffset(0), lineOffset(0) {
			Local<Object> options;
			if (maybe_options.ToLocal(&options)) {
				Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
				Local<Value> filename = Unmaybe(options->Get(context, v8_string("filename")));
				if (!filename->IsUndefined()) {
					if (!filename->IsString()) {
						throw js_type_error("`filename` must be a string");
					}
					this->filename = *String::Utf8Value(filename.As<String>());
				}
				Local<Value> columnOffset = Unmaybe(options->Get(context, v8_string("columnOffset")));
				if (!columnOffset->IsUndefined()) {
					if (!columnOffset->IsInt32()) {
						throw js_type_error("`columnOffset` must be an integer");
					}
					this->columnOffset = columnOffset.As<Int32>()->Value();
				}
				Local<Value> lineOffset = Unmaybe(options->Get(context, v8_string("lineOffset")));
				if (!lineOffset->IsUndefined()) {
					if (!lineOffset->IsInt32()) {
						throw js_type_error("`lineOffset` must be an integer");
					}
					this->lineOffset = lineOffset.As<Int32>()->Value();
				}
			}
		}

		ScriptOrigin ToScriptOrigin() {
			return ScriptOrigin(v8_string(filename.c_str()), Integer::New(Isolate::GetCurrent(), columnOffset), Integer::New(Isolate::GetCurrent(), lineOffset));
		}
};

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

				bool Check(const size_t length) {
					if (v8_heap + my_heap + length > next_check) {
						HeapStatistics heap_statistics;
						Isolate::GetCurrent()->GetHeapStatistics(&heap_statistics);
						v8_heap = heap_statistics.total_heap_size();
						if (v8_heap + my_heap + length > limit) {
							return false;
						}
						next_check = v8_heap + my_heap + length + 1024 * 1024;
					}
					if (v8_heap + my_heap + length > limit) {
						return false;
					}
					my_heap += length;
					return true;
				}

			public:
				LimitedAllocator(size_t limit) : limit(limit), v8_heap(1024 * 1024 * 4), my_heap(0), next_check(1024 * 1024) {}

				virtual void* Allocate(size_t length) {
					if (Check(length)) {
						return calloc(length, 1);
					} else {
						return nullptr;
					}
				}

				virtual void* AllocateUninitialized(size_t length) {
					if (Check(length)) {
						return malloc(length);
					} else {
						return nullptr;
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
			auto tmpl = Inherit<TransferableHandle>(MakeClass(
			 "Isolate", Parameterize<decltype(New), New>, 1,
				"compileScript", Parameterize<decltype(&IsolateHandle::CompileScript<true>), &IsolateHandle::CompileScript<true>>, 1,
				"compileScriptSync", Parameterize<decltype(&IsolateHandle::CompileScript<false>), &IsolateHandle::CompileScript<false>>, 1,
				"createContext", Parameterize<decltype(&IsolateHandle::CreateContext<true>), &IsolateHandle::CreateContext<true>>, 0,
				"createContextSync", Parameterize<decltype(&IsolateHandle::CreateContext<false>), &IsolateHandle::CreateContext<false>>, 0,
				"dispose", Parameterize<decltype(&IsolateHandle::Dispose), &IsolateHandle::Dispose>, 0
			));
			tmpl->Set(
				v8_string("createSnapshot"),
				FunctionTemplate::New(Isolate::GetCurrent(), Parameterize<decltype(CreateSnapshot), CreateSnapshot>, v8_string("createSnapshot"), Local<v8::Signature>(), 2)
			);
			return tmpl;
		}

		/**
		 * Create a new Isolate. It all starts here!
		 */
		static unique_ptr<ClassHandle> New(MaybeLocal<Object> maybe_options) {
			Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
			shared_ptr<ExternalCopyArrayBuffer> snapshot_blob;
			ResourceConstraints rc;
			size_t memory_limit = 128;

			// Parse options
			if (!maybe_options.IsEmpty()) {
				Local<Object> options = maybe_options.ToLocalChecked();

				// Check memory limits
				Local<Value> maybe_memory_limit = Unmaybe(options->Get(context, v8_symbol("memoryLimit")));
				if (!maybe_memory_limit->IsUndefined()) {
					if (!maybe_memory_limit->IsNumber()) {
						throw js_generic_error("`memoryLimit` must be a number");
					}
					memory_limit = maybe_memory_limit.As<Number>()->Value();
					if (memory_limit < 8) {
						throw js_generic_error("`memoryLimit` must be at least 8");
					}
				}

				// Set snapshot
				Local<Value> snapshot_handle = Unmaybe(options->Get(context, v8_symbol("snapshot")));
				if (!snapshot_handle->IsUndefined()) {
					if (
						!snapshot_handle->IsObject() ||
						!ClassHandle::GetFunctionTemplate<ExternalCopyHandle>()->HasInstance(snapshot_handle.As<Object>())
					) {
						throw js_type_error("`snapshot` must be an ExternalCopy to ArrayBuffer");
					}
					ExternalCopyHandle& copy_handle = *dynamic_cast<ExternalCopyHandle*>(ClassHandle::Unwrap(snapshot_handle.As<Object>()));
					snapshot_blob = std::dynamic_pointer_cast<ExternalCopyArrayBuffer>(copy_handle.Value());
					if (snapshot_blob.get() == nullptr) {
						throw js_type_error("`snapshot` must be an ExternalCopy to ArrayBuffer");
					}
				}
			}

			// Set memory limit
			rc.set_max_semi_space_size(std::pow(2, std::min(sizeof(void*) >= 8 ? 4 : 3, (int)(memory_limit / 128))));
			rc.set_max_old_space_size(memory_limit * 1.25);
			rc.set_max_executable_size(memory_limit * 0.75 + 0.5);
			auto allocator_ptr = unique_ptr<ArrayBuffer::Allocator>(new LimitedAllocator(memory_limit * 1024 * 1024));

			// Return isolate handle
			return std::make_unique<IsolateHandle>(std::make_shared<ShareableIsolate>(rc, std::move(allocator_ptr), std::move(snapshot_blob), memory_limit));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<IsolateHandleTransferable>(isolate);
		}

		/**
		 * Create a new v8::Context in this isolate and returns a ContextHandle
		 */
		template <bool async>
		Local<Value> CreateContext() {
			return ThreePhaseRunner<async>(*isolate, []() {
				return std::make_tuple();
			}, [this]() {
				// Make a new context and return the context and its global object
				Local<Context> context = Context::New(*isolate);
				return std::make_tuple(
					std::make_shared<ShareablePersistent<Context>>(context),
					std::make_shared<ShareablePersistent<Value>>(context->Global())
				);
			}, [](shared_ptr<ShareablePersistent<Context>> context, shared_ptr<ShareablePersistent<Value>> global) {
				// Make a new Context{} JS class
				return ClassHandle::NewInstance<ContextHandle>(context, global);
			});
		}

		/**
		 * Compiles a script in this isolate and returns a ScriptHandle
		 */
		template <bool async>
		Local<Value> CompileScript(Local<String> code_handle, MaybeLocal<Object> options) {
			return ThreePhaseRunner<async>(*isolate, [&code_handle, &options]() {
				// Copy code string out of first isolate
				return std::make_tuple(
					std::make_shared<ExternalCopyString>(code_handle),
					std::make_shared<ScriptOriginHolder>(options)
				);
			}, [this](shared_ptr<ExternalCopyString> code_copy, shared_ptr<ScriptOriginHolder> script_origin_holder) {
				// Compile in second isolate and return UnboundScript persistent
				Context::Scope context_scope(isolate->DefaultContext());
				Local<String> code_inner = code_copy->CopyInto().As<String>();
				ScriptOrigin script_origin = script_origin_holder->ToScriptOrigin();
				ScriptCompiler::Source source(code_inner, script_origin);
				Local<UnboundScript> script = Unmaybe(ScriptCompiler::CompileUnboundScript(*isolate, &source, ScriptCompiler::kNoCompileOptions));
				return std::make_shared<ShareablePersistent<UnboundScript>>(script);
			}, [this](shared_ptr<ShareablePersistent<UnboundScript>> script) {
				// Wrap UnboundScript in JS Script{} class
				return ClassHandle::NewInstance<ScriptHandle>(script);
			});
		}

		/**
		 * Dispose an isolate
		 */
		Local<Value> Dispose() {
			isolate->Dispose();
			return Undefined(Isolate::GetCurrent());
		}
};

/**
 * Create a snapshot from some code and return it as an external ArrayBuffer
 */
Local<Value> CreateSnapshot(Local<Array> script_handles, MaybeLocal<String> warmup_handle) {

	// Copy embed scripts and warmup script from outer isolate
	std::vector<std::pair<std::string, ScriptOriginHolder>> scripts;
	Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
	Local<Array> keys = Unmaybe(script_handles->GetOwnPropertyNames(context));
	scripts.reserve(keys->Length());
	for (uint32_t ii = 0; ii < keys->Length(); ++ii) {
		Local<Uint32> key = Unmaybe(Unmaybe(keys->Get(context, ii))->ToArrayIndex(context));
		if (key->Value() != ii) {
			throw js_type_error("Invalid `scripts` array");
		}
		Local<Value> script_handle = Unmaybe(script_handles->Get(context, key));
		if (!script_handle->IsObject()) {
			throw js_type_error("`scripts` should be array of objects");
		}
		Local<Value> script = Unmaybe(script_handle.As<Object>()->Get(context, v8_string("code")));
		if (!script->IsString()) {
			throw js_type_error("`code` property is required");
		}
		ScriptOriginHolder script_origin(script_handle.As<Object>());
		scripts.push_back(std::make_pair(std::string(*String::Utf8Value(script.As<String>())), script_origin));
	}
	std::string warmup_script;
	if (!warmup_handle.IsEmpty()) {
		warmup_script = *String::Utf8Value(warmup_handle.ToLocalChecked().As<String>());
	}

	// Create the snapshot
	StartupData snapshot;
	unique_ptr<const char> snapshot_data_ptr;
	{
		SnapshotCreator snapshot_creator;
		Isolate* isolate = snapshot_creator.GetIsolate();
		{
			HandleScope handle_scope(isolate);
			Local<Context> context = Context::New(isolate);
			{
				HandleScope handle_scope(isolate);
				Local<Context> context_dirty = Context::New(isolate);
				for (auto& script : scripts) {
					Local<String> code = v8_string(script.first.c_str());
					ScriptOrigin script_origin = script.second.ToScriptOrigin();
					ScriptCompiler::Source source(code, script_origin);
					Local<UnboundScript> unbound_script;
					{
						Context::Scope context_scope(context);
						Local<Script> compiled_script = Unmaybe(ScriptCompiler::Compile(context, &source, ScriptCompiler::kNoCompileOptions));
						Unmaybe(compiled_script->Run(context));
						unbound_script = compiled_script->GetUnboundScript();
					}
					{
						Context::Scope context_scope(context_dirty);
						Unmaybe(unbound_script->BindToCurrentContext()->Run(context_dirty));
					}
				}
				if (warmup_script.length() != 0) {
					Context::Scope context_scope(context_dirty);
					Unmaybe(Unmaybe(Script::Compile(context_dirty, v8_string(warmup_script.c_str())))->Run(context_dirty));
				}
			}
			isolate->ContextDisposedNotification(false);
			snapshot_creator.AddContext(context);
		}
		snapshot = snapshot_creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kKeep);
		snapshot_data_ptr.reset(snapshot.data);
	}

	// Export to outer scope
	if (snapshot.raw_size == 0) {
		throw js_generic_error("Failure creating snapshot");
	}
	auto buffer = std::make_shared<ExternalCopyArrayBuffer>((void*)snapshot.data, snapshot.raw_size);
	return ClassHandle::NewInstance<ExternalCopyHandle>(buffer);
}

}
