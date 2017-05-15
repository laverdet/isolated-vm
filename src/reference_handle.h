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
				ReferenceHandleTransferable(shared_ptr<ShareablePersistent<Value>>& reference, shared_ptr<ShareablePersistent<Context>>& context) : reference(reference), context(context) {}

				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ReferenceHandle>(reference, context);
				}
		};

	public:
		ReferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference, shared_ptr<ShareablePersistent<Context>>& context) : reference(reference), context(context) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Reference", New, 1,
				"deref", Method<ReferenceHandle, &ReferenceHandle::Deref>, 0,
				"derefInto", Method<ReferenceHandle, &ReferenceHandle::DerefInto>, 0,
				"getSync", Method<ReferenceHandle, &ReferenceHandle::GetSync>, 1,
				"setSync", Method<ReferenceHandle, &ReferenceHandle::SetSync>, 2,
				"applySync", Method<ReferenceHandle, &ReferenceHandle::ApplySync>, 2
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ReferenceHandleTransferable>(reference, context);
		}

		/**
		 * Make a new reference to a local handle.
		 */
		static void New(const FunctionCallbackInfo<Value>& args) {
			if (!args.IsConstructCall()) {
				THROW(Exception::TypeError, "Class constructor Reference cannot be invoked without 'new'");
			} else if (args.Length() < 1) {
				THROW(Exception::TypeError, "Class constructor Reference expects 1 parameter");
			}
			auto reference = std::make_shared<ShareablePersistent<Value>>(args[0]);
			auto context = std::make_shared<ShareablePersistent<Context>>(Isolate::GetCurrent()->GetCurrentContext());
			Wrap(std::make_unique<ReferenceHandle>(reference, context), args.This());
			args.GetReturnValue().Set(args.This());
		}

		/**
		 * Attempt to return this handle to the current context.
		 */
		void Deref(const FunctionCallbackInfo<Value>& args) {
			if (reference->GetIsolate() != Isolate::GetCurrent()) {
				THROW(Exception::TypeError, "Cannot dereference this from current isolate");
			}
			args.GetReturnValue().Set(reference->Deref());
		}

		/**
		 * Return a handle which will dereference itself when passing into another isolate.
		 */
		void DerefInto(const FunctionCallbackInfo<Value>& args) {
			args.GetReturnValue().Set(ClassHandle::NewInstance<DereferenceHandle>(reference));
		}

		/**
		 * Get a property from this reference, returned as another reference
		 */
		void GetSync(const FunctionCallbackInfo<Value>& args) {
			if (args.Length() < 1) {
				THROW(Exception::TypeError, "Method getSync expects 2 parameters");
			}
			unique_ptr<ExternalCopy> key = ExternalCopy::CopyIfPrimitive(args[0]);
			if (key.get() == nullptr) {
				THROW(Exception::TypeError, "key is invalid");
			}
			unique_ptr<Transferable> result = ShareableIsolate::Locker(reference->GetIsolate(), [ this, &key ]() -> unique_ptr<Transferable> {
				TryCatch try_catch(Isolate::GetCurrent());
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key->CopyInto();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				MaybeLocal<Value> result = object->Get(context, key_inner);
				if (try_catch.HasCaught()) {
					try_catch.ReThrow();
					return nullptr;
				}
				auto reference = std::make_shared<ShareablePersistent<Value>>(result.ToLocalChecked());
				return std::make_unique<ReferenceHandleTransferable>(reference, this->context);
			});
			if (result.get() != nullptr) {
				args.GetReturnValue().Set(result->TransferIn());
			}
		}

		/**
		 * Attempt to set a property on this reference
		 */
		void SetSync(const FunctionCallbackInfo<Value>& args) {
			if (args.Length() < 2) {
				THROW(Exception::TypeError, "Method setSync expects 2 parameters");
			}
			unique_ptr<ExternalCopy> key = ExternalCopy::CopyIfPrimitive(args[0]);
			if (key.get() == nullptr) {
				THROW(Exception::TypeError, "key is invalid");
			}
			unique_ptr<Transferable> val;
			{
				TryCatch try_catch(Isolate::GetCurrent());
				val = Transferable::TransferOut(args[1]);
				if (try_catch.HasCaught()) {
					try_catch.ReThrow();
					return;
				}
			}
			if (val.get() == nullptr) {
				THROW(Exception::TypeError, "value is invalid");
			}
			bool result = ShareableIsolate::Locker(reference->GetIsolate(), [ this, &key, &val ]() {
				TryCatch try_catch(Isolate::GetCurrent());
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> key_inner = key->CopyInto();
				Local<Value> val_inner = val->TransferIn();
				Local<Object> object = Local<Object>::Cast(reference->Deref());
				Maybe<bool> result = object->Set(context, key_inner, val_inner);
				if (try_catch.HasCaught()) {
					try_catch.ReThrow();
					return false;
				}
				return result.FromJust();
			});
			args.GetReturnValue().Set(Boolean::New(Isolate::GetCurrent(), result));
		}

		/**
		 * Call a function, like Function.prototype.apply
		 */
		void ApplySync(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			Local<Context> context = isolate->GetCurrentContext();

			// Get receiver, holder, this, whatever
			unique_ptr<Transferable> recv;
			if (args.Length() >= 1) {
				recv = Transferable::TransferOut(args[0]);
				if (recv.get() == nullptr) {
					return;
				}
			}

			// Externalize all arguments
			std::vector<unique_ptr<Transferable>> arguments;
			if (args.Length() >= 2 && args[1]->IsObject()) {
				Local<Object> list = args[1].As<Object>();
				MaybeLocal<Array> maybe_keys = list->GetOwnPropertyNames(isolate->GetCurrentContext());
				Local<Array> keys;
				if (!maybe_keys.ToLocal(&keys)) {
					return;
				}
				arguments.reserve(keys->Length());
				for (uint32_t ii = 0; ii < keys->Length(); ++ii) {
					Local<Value> key = keys->Get(ii);
					if (!key->IsNumber() || key.As<Uint32>()->Value() != ii) {
						THROW(Exception::TypeError, "Invalid argument array");
					}
					MaybeLocal<Value> val = list->Get(context, key);
					if (val.IsEmpty()) {
						return;
					}
					unique_ptr<Transferable> val_out = Transferable::TransferOut(val.ToLocalChecked());
					if (val_out.get() == nullptr) {
						return;
					}
					arguments.push_back(std::move(val_out));
				}
			}

			// Invoke in the isolate
			unique_ptr<Transferable> result = ShareableIsolate::Locker(reference->GetIsolate(), [ this, &recv, &arguments ]() -> unique_ptr<Transferable> {
				Local<Context> context = this->context->Deref();
				Context::Scope context_scope(context);
				Local<Value> fn = reference->Deref();
				if (!fn->IsFunction()) {
					Isolate::GetCurrent()->ThrowException(Exception::TypeError(v8_string("Reference is not a function")));
					return nullptr;
				}
				TryCatch try_catch(Isolate::GetCurrent());
				Local<Value> recv_inner = recv->TransferIn();
				Local<Value> arguments_inner[arguments.size()];
				for (size_t ii = 0; ii < arguments.size(); ++ii) {
					arguments_inner[ii] = arguments[ii]->TransferIn();
				}
				MaybeLocal<Value> result = fn.As<Function>()->Call(context, recv_inner, arguments.size(), arguments_inner);
				if (try_catch.HasCaught()) {
					try_catch.ReThrow();
					return nullptr;
				}
				return Transferable::TransferOut(result.ToLocalChecked());
			});
			if (result.get() == nullptr) {
				return;
			}
			args.GetReturnValue().Set(result->TransferIn());
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
						isolate->ThrowException(Exception::Error(v8_string("Cannot dereference this into target isolate")));
						return Local<Value>();
					}
				}
		};

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass("Dereference", New, 0));
		}

		DereferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference) : reference(reference) {}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Context is private");
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<DereferenceHandleTransferable>(reference);
		}

};

}
