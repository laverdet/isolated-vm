#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "script_handle.h"
#include "external_copy.h"
#include "external_copy_handle.h"
#include "session_handle.h"

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

				size_t GetAllocatedSize() const {
					return my_heap;
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
			 "Isolate", ParameterizeCtor<decltype(&New), &New>, 1,
				"compileScript", Parameterize<decltype(&IsolateHandle::CompileScript<true>), &IsolateHandle::CompileScript<true>>, 1,
				"compileScriptSync", Parameterize<decltype(&IsolateHandle::CompileScript<false>), &IsolateHandle::CompileScript<false>>, 1,
				"createContext", Parameterize<decltype(&IsolateHandle::CreateContext<true>), &IsolateHandle::CreateContext<true>>, 0,
				"createContextSync", Parameterize<decltype(&IsolateHandle::CreateContext<false>), &IsolateHandle::CreateContext<false>>, 0,
				"createInspectorSession", Parameterize<decltype(&IsolateHandle::CreateInspectorSession), &IsolateHandle::CreateInspectorSession>, 0,
				"dispose", Parameterize<decltype(&IsolateHandle::Dispose), &IsolateHandle::Dispose>, 0,
				"getHeapStatistics", Parameterize<decltype(&IsolateHandle::GetHeapStatistics), &IsolateHandle::GetHeapStatistics>, 0
			));
			tmpl->Set(
				v8_string("createSnapshot"),
				FunctionTemplate::New(Isolate::GetCurrent(), ParameterizeStatic<decltype(&CreateSnapshot), &CreateSnapshot>, v8_string("createSnapshot"), Local<v8::Signature>(), 2)
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
					memory_limit = (size_t)maybe_memory_limit.As<Number>()->Value();
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
					snapshot_blob = std::dynamic_pointer_cast<ExternalCopyArrayBuffer>(copy_handle.GetValue());
					if (snapshot_blob.get() == nullptr) {
						throw js_type_error("`snapshot` must be an ExternalCopy to ArrayBuffer");
					}
				}
			}

			// Set memory limit
			rc.set_max_semi_space_size((int)std::pow(2, std::min(sizeof(void*) >= 8 ? 4 : 3, (int)(memory_limit / 128))));
			rc.set_max_old_space_size((int)(memory_limit * 1.25));
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
			class CreateContext : public ThreePhaseTask {
				private:
					// phase 2 -> 3
					shared_ptr<ShareableContext> context;
					shared_ptr<ShareablePersistent<Value>> global;

				protected:
					void Phase2() final {
						// Make a new context and return the context and its global object
						Local<Context> context_handle = Context::New(Isolate::GetCurrent());
						context = std::make_shared<ShareableContext>(context_handle);
						global = std::make_shared<ShareablePersistent<Value>>(context_handle->Global());
					}

					Local<Value> Phase3() final {
						// Make a new Context{} JS class
						return ClassHandle::NewInstance<ContextHandle>(std::move(context), std::move(global));
					}
			};
			return ThreePhaseTask::Run<async, CreateContext>(*this->isolate);
		}

		/**
		 * Compiles a script in this isolate and returns a ScriptHandle
		 */
		template <bool async>
		Local<Value> CompileScript(Local<String> code_handle, MaybeLocal<Object> maybe_options) {
			class CompileScript : public ThreePhaseTask {
				private:
					// phase 2
					ShareableIsolate* isolate;
					unique_ptr<ExternalCopyString> code_string;
					unique_ptr<ScriptOriginHolder> script_origin_holder;
					shared_ptr<ExternalCopyArrayBuffer> cached_data_blob; // also phase 3
					bool produce_cached_data { false };
					// phase 3
					shared_ptr<ShareablePersistent<UnboundScript>> script;
					bool supplied_cached_data { false };
					bool cached_data_rejected { false };

				public:
					CompileScript(const Local<String>& code_handle, const MaybeLocal<Object>& maybe_options, ShareableIsolate* isolate) : isolate(isolate) {
						// Read options
						Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
						Local<Object> options;

						script_origin_holder = std::make_unique<ScriptOriginHolder>(maybe_options);
						if (maybe_options.ToLocal(&options)) {

							// Get cached data blob
							Local<Value> cached_data_handle = Unmaybe(options->Get(context, v8_symbol("cachedData")));
							if (!cached_data_handle->IsUndefined()) {
								if (
									!cached_data_handle->IsObject() ||
									!ClassHandle::GetFunctionTemplate<ExternalCopyHandle>()->HasInstance(cached_data_handle.As<Object>())
								) {
									throw js_type_error("`cachedData` must be an ExternalCopy to ArrayBuffer");
								}
								ExternalCopyHandle& copy_handle = *dynamic_cast<ExternalCopyHandle*>(ClassHandle::Unwrap(cached_data_handle.As<Object>()));
								cached_data_blob = std::dynamic_pointer_cast<ExternalCopyArrayBuffer>(copy_handle.GetValue());
								if (!cached_data_blob) {
									throw js_type_error("`cachedData` must be an ExternalCopy to ArrayBuffer");
								}
							}

							// Get cached data flag
							Local<Value> produce_cached_data_val;
							if (
								options->Get(
									context, v8_symbol("produceCachedData")
								).ToLocal(&produce_cached_data_val)
							) {
								produce_cached_data = produce_cached_data_val->IsTrue();
							}
						}

						// Copy code string
						code_string = std::make_unique<ExternalCopyString>(code_handle);
					}

					void Phase2() final {
						// Compile in second isolate and return UnboundScript persistent
						ShareableIsolate* isolate = this->isolate;
						Context::Scope context_scope(isolate->DefaultContext());
						Local<String> code_inner = code_string->CopyInto().As<String>();
						code_string.reset();
						ScriptOrigin script_origin = script_origin_holder->ToScriptOrigin();
						script_origin_holder.reset();
						ScriptCompiler::CompileOptions compile_options = ScriptCompiler::kNoCompileOptions;
						unique_ptr<ScriptCompiler::CachedData> cached_data = nullptr;
						if (cached_data_blob) {
							compile_options = ScriptCompiler::kConsumeCodeCache;
							cached_data = std::make_unique<ScriptCompiler::CachedData>((const uint8_t*)cached_data_blob->Data(), cached_data_blob->Length());
						} else if (produce_cached_data) {
							compile_options = ScriptCompiler::kProduceCodeCache;
						}
						ScriptCompiler::Source source(code_inner, script_origin, cached_data.release());
						script = std::make_shared<ShareablePersistent<UnboundScript>>(RunWithAnnotatedErrors<Local<UnboundScript>>(
							[&isolate, &source, compile_options]() { return Unmaybe(ScriptCompiler::CompileUnboundScript(*isolate, &source, compile_options)); }
						));

						// Check cached data flags
						if (cached_data_blob) {
							supplied_cached_data = true;
							cached_data_rejected = source.GetCachedData()->rejected;
							cached_data_blob.reset();
						} else if (produce_cached_data) {
							const ScriptCompiler::CachedData* cached_data = source.GetCachedData();
							assert(cached_data != nullptr);
							cached_data_blob = std::make_shared<ExternalCopyArrayBuffer>((void*)cached_data->data, cached_data->length);
						}
					}

					Local<Value> Phase3() final {
						// Wrap UnboundScript in JS Script{} class
						Local<Object> value = ClassHandle::NewInstance<ScriptHandle>(std::move(script));
						Isolate* isolate = Isolate::GetCurrent();
						if (supplied_cached_data) {
							value->Set(v8_symbol("cachedDataRejected"), Boolean::New(isolate, cached_data_rejected));
						} else if (cached_data_blob) {
							value->Set(v8_symbol("cachedData"), ClassHandle::NewInstance<ExternalCopyHandle>(cached_data_blob));
						}
						return value;
					}
			};
			return ThreePhaseTask::Run<async, CompileScript>(*this->isolate, code_handle, maybe_options, this->isolate.get());
		}

		/**
		 * Create a new channel for debugging on the inspector
		 */
		Local<Value> CreateInspectorSession() {
			if (&ShareableIsolate::GetCurrent() == isolate.get()) {
				throw js_generic_error("An isolate is not debuggable from within itself");
			}
			return ClassHandle::NewInstance<SessionHandle>(isolate.get());
		}

		/**
		 * Dispose an isolate
		 */
		Local<Value> Dispose() {
			isolate->Dispose();
			return Undefined(Isolate::GetCurrent());
		}

		/**
		 * Get heap statistics from v8
		 */
		Local<Value> GetHeapStatistics() {
			HeapStatistics heap = isolate->GetHeapStatistics();
			LimitedAllocator* allocator = static_cast<LimitedAllocator*>(isolate->GetAllocator());
			Local<Object> ret = Object::New(Isolate::GetCurrent());
			ret->Set(v8_string("total_heap_size"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.total_heap_size()));
			ret->Set(v8_string("total_heap_size_executable"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.total_heap_size_executable()));
			ret->Set(v8_string("total_physical_size"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.total_physical_size()));
			ret->Set(v8_string("total_available_size"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.total_available_size()));
			ret->Set(v8_string("used_heap_size"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.used_heap_size()));
			ret->Set(v8_string("heap_size_limit"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.heap_size_limit()));
			ret->Set(v8_string("malloced_memory"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.malloced_memory()));
			ret->Set(v8_string("peak_malloced_memory"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.peak_malloced_memory()));
			ret->Set(v8_string("does_zap_garbage"), Integer::NewFromUnsigned(Isolate::GetCurrent(), heap.does_zap_garbage()));
			ret->Set(v8_string("externally_allocated_size"), Integer::NewFromUnsigned(Isolate::GetCurrent(), allocator->GetAllocatedSize()));
			return ret;
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
	std::shared_ptr<ExternalCopy> error;
	{
		SnapshotCreator snapshot_creator;
		Isolate* isolate = snapshot_creator.GetIsolate();
		{
			Locker locker(isolate);
			TryCatch try_catch(isolate);
			HandleScope handle_scope(isolate);
			Local<Context> context = Context::New(isolate);
			snapshot_creator.SetDefaultContext(context);
			try {
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
							Local<Script> compiled_script = RunWithAnnotatedErrors<Local<Script>>(
								[&context, &source]() { return Unmaybe(ScriptCompiler::Compile(context, &source, ScriptCompiler::kNoCompileOptions)); }
							);
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
						MaybeLocal<Object> tmp;
						ScriptOriginHolder script_origin(tmp);
						ScriptCompiler::Source source(v8_string(warmup_script.c_str()), script_origin.ToScriptOrigin());
						RunWithAnnotatedErrors<void>([&context_dirty, &source]() {
							Unmaybe(Unmaybe(ScriptCompiler::Compile(context_dirty, &source, ScriptCompiler::kNoCompileOptions))->Run(context_dirty));
						});
					}
				}
				isolate->ContextDisposedNotification(false);
				snapshot_creator.AddContext(context);
			} catch (const js_error_base& cc_error) {
				assert(try_catch.HasCaught());
				HandleScope handle_scope(isolate);
				Context::Scope context_scope(context);
				error = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
			}
		}
		if (error.get() == nullptr) {
			snapshot = snapshot_creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kKeep);
			snapshot_data_ptr.reset(snapshot.data);
		}
	}

	// Export to outer scope
	if (error.get() != nullptr) {
		Isolate::GetCurrent()->ThrowException(error->CopyInto());
		return Undefined(Isolate::GetCurrent());
	} else if (snapshot.raw_size == 0) {
		throw js_generic_error("Failure creating snapshot");
	}
	auto buffer = std::make_shared<ExternalCopyArrayBuffer>((void*)snapshot.data, snapshot.raw_size);
	return ClassHandle::NewInstance<ExternalCopyHandle>(buffer);
}

}
