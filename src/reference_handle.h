#pragma once
#include <node.h>
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"

#include <memory>

namespace ivm {

using namespace v8;
using std::shared_ptr;
class DereferenceHandle;

/**
 * This will be a reference to any v8 Value in any isolate.
 */
class ReferenceHandle : public ClassHandle {
	friend class Transferable;

	private:
		shared_ptr<ShareablePersistent<Value>> reference;
		shared_ptr<ShareablePersistent<Context>> context;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass(
				"Reference", New, 1,
				"deref", Method<ReferenceHandle, &ReferenceHandle::Deref>, 0,
				"derefInto", Method<ReferenceHandle, &ReferenceHandle::DerefInto>, 0,
				"setSync", Method<ReferenceHandle, &ReferenceHandle::SetSync>, 2
			);
		}

		ReferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference, shared_ptr<ShareablePersistent<Context>>& context) : reference(reference), context(context) {}

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
			Transfer(std::make_unique<ReferenceHandle>(reference, context), args.This());
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
			TryCatch try_catch(Isolate::GetCurrent());
			unique_ptr<Transferable> val = Transferable::TransferOut(args[1]);
			if (try_catch.HasCaught()) {
				try_catch.ReThrow();
				return;
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
					return false;
				}
				return result.FromJust();
			});
			if (try_catch.HasCaught()) {
				try_catch.ReThrow();
				return;
			}
			args.GetReturnValue().Set(Boolean::New(Isolate::GetCurrent(), result));
		}
};

/**
 * The return value for .derefInto()
 */
class DereferenceHandle : public ClassHandle {
	friend class Transferable;

	private:
		shared_ptr<ShareablePersistent<Value>> reference;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass("Dereference", New, 0);
		}

		DereferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference) : reference(reference) {}

		static void New(const FunctionCallbackInfo<Value>& args) {
      THROW(Exception::TypeError, "Constructor Context is private");
		}
};

}
