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
				"getSync", Parameterize<decltype(&ReferenceHandle::GetSync), &ReferenceHandle::GetSync>, 1,
				"setSync", Parameterize<decltype(&ReferenceHandle::SetSync), &ReferenceHandle::SetSync>, 2,
				"applySync", Parameterize<decltype(&ReferenceHandle::ApplySync), &ReferenceHandle::ApplySync>, 2
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
		Local<Value> GetSync(Local<Value> key) {
			unique_ptr<ExternalCopy> key_copy = ExternalCopy::CopyIfPrimitive(key);
			if (key_copy.get() == nullptr) {
				throw js_type_error("Invalid `key`");
			}
			return ShareableIsolate::Locker(reference->GetIsolate(), [ this, &key_copy ]() {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key_copy->CopyInto();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				return std::make_unique<ReferenceHandleTransferable>(
					std::make_shared<ShareablePersistent<Value>>(Unmaybe(object->Get(context, key_inner))),
					this->context
				);
			})->TransferIn();
		}

		/**
		 * Attempt to set a property on this reference
		 */
		Local<Value> SetSync(Local<Value> key, Local<Value> val) {
			unique_ptr<ExternalCopy> key_copy = ExternalCopy::CopyIfPrimitive(key);
			if (key_copy.get() == nullptr) {
				throw js_type_error("Invalid `key`");
			}
			unique_ptr<Transferable> val_ref = Transferable::TransferOut(val);
			return Boolean::New(Isolate::GetCurrent(), ShareableIsolate::Locker(reference->GetIsolate(), [ this, &key_copy, &val_ref ]() {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key_copy->CopyInto();
				Local<Value> val_inner = val_ref->TransferIn();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				return Unmaybe(object->Set(context, key_inner, val_inner));
			}));
		}

		/**
		 * Call a function, like Function.prototype.apply
		 */
		Local<Value> ApplySync(Local<Value> recv, MaybeLocal<Array> maybe_arguments) {
			Isolate* isolate = Isolate::GetCurrent();
			Local<Context> context = isolate->GetCurrentContext();

			// Get receiver, holder, this, whatever
			unique_ptr<Transferable> recv_ref = Transferable::TransferOut(recv);

			// Externalize all arguments
			std::vector<unique_ptr<Transferable>> argv;
			if (!maybe_arguments.IsEmpty()) {
				Local<Array> arguments = maybe_arguments.ToLocalChecked();
				Local<Array> keys = Unmaybe(arguments->GetOwnPropertyNames(isolate->GetCurrentContext()));
				argv.reserve(keys->Length());
				for (uint32_t ii = 0; ii < keys->Length(); ++ii) {
					Local<Uint32> key = Unmaybe(Unmaybe(keys->Get(context, ii))->ToArrayIndex(context));
					if (key->Value() != ii) {
						throw js_type_error("Invalid `arguments` array");
					}
					argv.push_back(Transferable::TransferOut(Unmaybe(arguments->Get(context, key))));
				}
			}

			// Invoke in the isolate
			return ShareableIsolate::Locker(reference->GetIsolate(), [ this, &recv_ref, &argv ]() {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> fn = reference->Deref();
				if (!fn->IsFunction()) {
					throw js_type_error("Reference is not a function");
				}
				Local<Value> recv_inner = recv_ref->TransferIn();
				Local<Value> argv_inner[argv.size()];
				for (size_t ii = 0; ii < argv.size(); ++ii) {
					argv_inner[ii] = argv[ii]->TransferIn();
				}
				return Transferable::TransferOut(Unmaybe(fn.As<Function>()->Call(context, recv_inner, argv.size(), argv_inner)));
			})->TransferIn();
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
