#pragma once
#include <node.h>
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"

#include <memory>
#include <vector>

namespace ivm {

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;
class DereferenceHandle;

/**
 * This will be a reference to any v8 Value in any isolate.
 */
class ReferenceHandle : public TransferableHandle {
	friend class Transferable;

	private:
		shared_ptr<ShareablePersistent<Value>> reference;
		shared_ptr<ShareablePersistent<Context>> context;

		/**
		 * Wrapper class created when you pass a ReferenceHandle through to another isolate
		 */
		class ReferenceHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareablePersistent<Value>> reference;
				shared_ptr<ShareablePersistent<Context>> context;

			public:
				ReferenceHandleTransferable(shared_ptr<ShareablePersistent<Value>> reference, shared_ptr<ShareablePersistent<Context>>& context) : reference(reference), context(context) {}

				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ReferenceHandle>(reference, context);
				}
		};

	public:
		ReferenceHandle(shared_ptr<ShareablePersistent<Value>> reference, shared_ptr<ShareablePersistent<Context>> context) : reference(reference), context(context) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Reference", Parameterize<decltype(New), New>, 1,
				"deref", Parameterize<decltype(&ReferenceHandle::Deref), &ReferenceHandle::Deref>, 0,
				"derefInto", Parameterize<decltype(&ReferenceHandle::DerefInto), &ReferenceHandle::DerefInto>, 0,
				"get", Parameterize<decltype(&ReferenceHandle::Get<true>), &ReferenceHandle::Get<true>>, 1,
				"getSync", Parameterize<decltype(&ReferenceHandle::Get<false>), &ReferenceHandle::Get<false>>, 1,
				"set", Parameterize<decltype(&ReferenceHandle::Set<true>), &ReferenceHandle::Set<true>>, 2,
				"setSync", Parameterize<decltype(&ReferenceHandle::Set<false>), &ReferenceHandle::Set<false>>, 2,
				"apply", Parameterize<decltype(&ReferenceHandle::Apply<true>), &ReferenceHandle::Apply<true>>, 2,
				"applySync", Parameterize<decltype(&ReferenceHandle::Apply<false>), &ReferenceHandle::Apply<false>>, 2
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ReferenceHandleTransferable>(reference, context);
		}

		/**
		 * Make a new reference to a local handle.
		 */
		static unique_ptr<ReferenceHandle> New(Local<Value> var) {
			return std::make_unique<ReferenceHandle>(
				std::make_shared<ShareablePersistent<Value>>(var),
				std::make_shared<ShareablePersistent<Context>>(Isolate::GetCurrent()->GetCurrentContext())
			);
		}

		/**
		 * Attempt to return this handle to the current context.
		 */
		Local<Value> Deref() {
			if (reference->GetIsolate() != Isolate::GetCurrent()) {
				throw js_type_error("Cannot dereference this from current isolate");
			}
			return reference->Deref();
		}

		/**
		 * Return a handle which will dereference itself when passing into another isolate.
		 */
		Local<Value> DerefInto() {
			return ClassHandle::NewInstance<DereferenceHandle>(reference);
		}

		/**
		 * Get a property from this reference, returned as another reference
		 */
		template <bool async>
		Local<Value> Get(Local<Value> key_handle) {
			return ThreePhaseRunner<async>(reference->GetIsolate(), [this, &key_handle]() {
				shared_ptr<ExternalCopy> key = ExternalCopy::CopyIfPrimitive(key_handle);
				if (key.get() == nullptr) {
					throw js_type_error("Invalid `key`");
				}
				return std::move(key);
			}, [this] (shared_ptr<ExternalCopy> key) {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key->CopyInto();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				return std::make_shared<ReferenceHandleTransferable>(
					std::make_shared<ShareablePersistent<Value>>(Unmaybe(object->Get(context, key_inner))),
					this->context
				);
			}, [] (shared_ptr<ReferenceHandleTransferable> ref) {
				return ref->TransferIn();
			});
		}

		/**
		 * Attempt to set a property on this reference
		 */
		template <bool async>
		Local<Value> Set(Local<Value> key_handle, Local<Value> val_handle) {
			return ThreePhaseRunner<async>(reference->GetIsolate(), [this, &key_handle, &val_handle]() {
				shared_ptr<ExternalCopy> key = ExternalCopy::CopyIfPrimitive(key_handle);
				if (key.get() == nullptr) {
					throw js_type_error("Invalid `key`");
				}
				shared_ptr<Transferable> val = Transferable::TransferOut(val_handle);
				return std::make_tuple(std::move(key), std::move(val));
			}, [this](shared_ptr<ExternalCopy> key, shared_ptr<Transferable> val) {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key->CopyInto();
				Local<Value> val_inner = val->TransferIn();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				return Unmaybe(object->Set(context, key_inner, val_inner));
			}, [](bool did_set) {
				return Boolean::New(Isolate::GetCurrent(), did_set);
			});
		}

		/**
		 * Call a function, like Function.prototype.apply
		 */
		template <bool async>
		Local<Value> Apply(Local<Value> recv_handle, MaybeLocal<Array> maybe_arguments) {
			return ThreePhaseRunner<async>(reference->GetIsolate(), [this, &recv_handle, &maybe_arguments]() {

				// Get receiver, holder, this, whatever
				auto recv = shared_ptr<Transferable>(Transferable::TransferOut(recv_handle));

				// Externalize all arguments
				std::vector<shared_ptr<Transferable>> argv;
				if (!maybe_arguments.IsEmpty()) {
					Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
					Local<Array> arguments = maybe_arguments.ToLocalChecked();
					Local<Array> keys = Unmaybe(arguments->GetOwnPropertyNames(context));
					argv.reserve(keys->Length());
					for (uint32_t ii = 0; ii < keys->Length(); ++ii) {
						Local<Uint32> key = Unmaybe(Unmaybe(keys->Get(context, ii))->ToArrayIndex(context));
						if (key->Value() != ii) {
							throw js_type_error("Invalid `arguments` array");
						}
						argv.push_back(Transferable::TransferOut(Unmaybe(arguments->Get(context, key))));
					}
				}
				return std::make_tuple(std::move(recv), std::move(argv));
			}, [this](shared_ptr<Transferable> recv, std::vector<shared_ptr<Transferable>> argv) {

				// Invoke in the isolate
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> fn = reference->Deref();
				if (!fn->IsFunction()) {
					throw js_type_error("Reference is not a function");
				}
				Local<Value> recv_inner = recv->TransferIn();
				Local<Value> argv_inner[argv.size()];
				for (size_t ii = 0; ii < argv.size(); ++ii) {
					argv_inner[ii] = argv[ii]->TransferIn();
				}
				Local<Value> result = Unmaybe(fn.As<Function>()->Call(context, recv_inner, argv.size(), argv_inner));
				return shared_ptr<Transferable>(Transferable::TransferOut(result));
			}, [](shared_ptr<Transferable> value) {

				// Copy result into original isolate
				return value->TransferIn();
			});
		}

};

/**
 * The return value for .derefInto()
 */
class DereferenceHandle : public TransferableHandle {
	friend class Transferable;

	private:
		shared_ptr<ShareablePersistent<Value>> reference;

		class DereferenceHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareablePersistent<Value>> reference;

			public:
				DereferenceHandleTransferable(shared_ptr<ShareablePersistent<Value>>& reference) : reference(reference) {}

				virtual Local<Value> TransferIn() {
					Isolate* isolate = Isolate::GetCurrent();
					if (isolate == reference->GetIsolate()) {
						return reference->Deref();
					} else {
						throw js_type_error("Cannot dereference this into target isolate");
					}
				}
		};

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass("Dereference", nullptr, 0));
		}

		DereferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference) : reference(reference) {}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<DereferenceHandleTransferable>(reference);
		}

};

}
