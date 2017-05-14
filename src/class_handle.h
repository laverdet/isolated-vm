#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"

#include <assert.h>

namespace ivm {
using namespace v8;

/**
 * Analogous to node::ObjectWrap but Isolate-aware. Also has a bunch of unnecessary template stuff.
 */
class ClassHandle {
	private:
		Persistent<Object> handle;

		/**
		 * Utility methods to set up object prototype
		 */
		static void AddMethod(Isolate* isolate, Local<ObjectTemplate>& proto, Local<Signature>& sig, const char* name, FunctionCallback impl, int length) {
			proto->Set(
				String::NewFromOneByte(isolate, (const uint8_t*)name, NewStringType::kInternalized).ToLocalChecked(),
				FunctionTemplate::New(isolate, impl, Local<Value>(), sig, length)
			);
		}

		template <typename... Args>
		static void AddMethods(Isolate* isolate, Local<ObjectTemplate>& proto, Local<Signature>& sig, const char* name, FunctionCallback impl, int length, Args... args) {
			AddMethod(isolate, proto, sig, name, impl, length);
			AddMethods(isolate, proto, sig, args...);
		}

		static void AddMethods(Isolate* isolate, Local<ObjectTemplate>& proto, Local<Signature>& sig) {
		}

		/**
		 * Convenience wrapper for the obtuse SetWeak function signature. When the callback is called
		 * the handle will already be gone.
		 */
		template <typename P, void (*F)(P*)>
		void SetWeak(P* param) {
			handle.SetWeak(param, WeakCallback<P, F>, WeakCallbackType::kParameter);
		}
		template <typename P, void (*F)(P*)>
		static void WeakCallback(const WeakCallbackInfo<P>& info) {
			F(info.GetParameter());
		}

		/**
		 * Invoked once JS loses all references to this object
		 */
		static void WeakCallback(ClassHandle* that) {
			delete that;
		}

	protected:
		/**
		 * Sets up this object's FunctionTemplate inside the current isolate
		 */
		template <typename... Args>
		static Local<FunctionTemplate> MakeClass(const char* class_name, FunctionCallback New, int length, Args... args) {
			Isolate* isolate = Isolate::GetCurrent();
			Local<FunctionTemplate> tmpl = FunctionTemplate::New(
				isolate, New,
				Local<Value>(), Local<Signature>(), length
			);
			tmpl->SetClassName(String::NewFromOneByte(isolate, (const uint8_t*)class_name, NewStringType::kInternalized).ToLocalChecked());
			tmpl->InstanceTemplate()->SetInternalFieldCount(1);

			Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
			Local<Signature> sig = Signature::New(isolate, tmpl);
			AddMethods(isolate, proto, sig, args...);

			return tmpl;
		}

		/**
		 * Automatically unwraps the C++ pointer and calls your class method with proper `this`
		 */
		template <typename T, void (T::* F)(const FunctionCallbackInfo<Value>&)>
		static void Method(const FunctionCallbackInfo<Value>& args) {
			T* that = Unwrap<T>(args.This());
			if (that == nullptr) {
				THROW(Exception::TypeError, "Object is not valid");
			}
			(static_cast<T*>(that)->*F)(args);
		}

	public:
		ClassHandle() {}
		virtual ~ClassHandle() {
			if (!handle.IsEmpty()) {
				handle.ClearWeak();
				handle.Reset();
			}
		}

		/**
		 * Returns instance of this class for this context.
		 */
		template <typename T>
		static Local<Function> Init() {
			return GetFunctionTemplate<T>()->GetFunction();
		}

		/**
		 * Returns the FunctionTemplate for this isolate, generating it if needed.
		 */
		template <typename T>
		static Local<FunctionTemplate> GetFunctionTemplate() {
    	ShareableIsolate::IsolateSpecific<FunctionTemplate>& specific = T::TemplateSpecific();
			MaybeLocal<FunctionTemplate> maybe_tmpl = specific.Deref();
			Local<FunctionTemplate> tmpl;
			if (maybe_tmpl.ToLocal(&tmpl)) {
				return tmpl;
			} else {
				tmpl = T::Definition();
				specific.Reset(tmpl);
				return tmpl;
			}
		}

		/**
		 * Builds a new instance of T from scratch, used in factory functions.
		 */
		template <typename T, typename ...Args>
		static Local<Object> NewInstance(Args... args) {
			Local<Object> instance = GetFunctionTemplate<T>()->InstanceTemplate()->NewInstance(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
			auto ptr = std::make_unique<T>(args...);
			Transfer(std::move(ptr), instance);
			return instance;
		}

		/**
		 * Transfer ownership of this C++ pointer to the v8 handle lifetime.
		 */
		static void Transfer(std::unique_ptr<ClassHandle> ptr, Local<Object> handle) {
			handle->SetAlignedPointerInInternalField(0, ptr.get());
			ptr->handle.Reset(Isolate::GetCurrent(), handle);
			ptr->SetWeak<ClassHandle, WeakCallback>(ptr.release());
		}

		/**
		 * Pull out native pointer from v8 handle
		 */
		template <typename T>
		static T* Unwrap(Local<Object> handle) {
			assert(!handle.IsEmpty());
			assert(handle->InternalFieldCount() > 0);
			ClassHandle* that = static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0));
			return static_cast<T*>(that);
		}
};

}
