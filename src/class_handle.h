#pragma once
#include <node.h>
#include "util.h"
#include "convert_param.h"
#include "shareable_isolate.h"
#include "shareable_persistent.h"

#include <assert.h>
#include <stdexcept>

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
			Local<String> name_handle = v8_symbol(name);
			proto->Set(name_handle, FunctionTemplate::New(isolate, impl, name_handle, sig, length));
		}

		template <typename... Args>
		static void AddMethods(Isolate* isolate, Local<ObjectTemplate>& proto, Local<Signature>& sig, const char* name, FunctionCallback impl, int length, Args... args) {
			AddMethod(isolate, proto, sig, name, impl, length);
			AddMethods(isolate, proto, sig, args...);
		}

		static void AddMethods(Isolate* isolate, Local<ObjectTemplate>& proto, Local<Signature>& sig) {
		}

		/**
		 * Below is the Parameterize<> template magic
		 */
		// These two templates will peel arguments off and run them through ConvertParam<T>
		template <int O, typename FnT, typename R, typename ConvertedArgsT>
		static inline R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, const FunctionCallbackInfo<Value>& info) {
			return apply_from_tuple(fn, convertedArgs);
		}

		template <int O, typename FnT, typename R, typename ConvertedArgsT, typename T, typename... ConversionArgsRest>
		static inline R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, const FunctionCallbackInfo<Value>& info) {
			auto t = std::tuple_cat(convertedArgs, std::make_tuple(
				ConvertParam<T>::Convert(
					(int)(std::tuple_size<ConvertedArgsT>::value) + O,
					(int)(sizeof...(ConversionArgsRest) + std::tuple_size<ConvertedArgsT>::value) + O + 1,
					info
				)
			));
			return ParameterizeHelper<O, FnT, R, decltype(t), ConversionArgsRest...>(fn, t, info);
		}

		// Regular function that just returns a Local<Value>
		template <int O, typename ...Args>
		static inline Local<Value> ParameterizeHelperStart(const FunctionCallbackInfo<Value>& info, Local<Value>(*fn)(Args...)) {
			return ParameterizeHelper<O, Local<Value>(*)(Args...), Local<Value>, std::tuple<>, Args...>(fn, std::tuple<>(), info);
		}

		// Constructor functions need to pair the C++ instance to the v8 handle
		template <int O, typename R, typename ...Args>
		static inline Local<Value> ParameterizeHelperCtorStart(const FunctionCallbackInfo<Value>& info, std::unique_ptr<R>(*fn)(Args...)) {
			RequireConstructorCall(info);
			std::unique_ptr<R> result = ParameterizeHelper<O, std::unique_ptr<R>(*)(Args...), std::unique_ptr<R>, std::tuple<>, Args...>(fn, std::tuple<>(), info);
			if (result.get() != nullptr) {
				Local<Object> handle = info.This().As<Object>();
				Wrap(std::move(result), handle);
				return handle;
			} else {
				return Undefined(Isolate::GetCurrent());
			}
		}

		// Main entry point for parameterized functions
		template <int O, typename T, T F>
		static inline void ParameterizeEntry(const FunctionCallbackInfo<Value>& info) {
			try {
				Local<Value> result = ParameterizeHelperStart<O>(info, F);
				if (result.IsEmpty()) {
					throw std::runtime_error("Member function returned empty Local<> but did not set exception");
				}
				info.GetReturnValue().Set(result);
			} catch (const js_error_base& err) {}
		}

		// Main entry point for parameterized constructors
		template <int O, typename T, T F>
		static inline void ParameterizeCtorEntry(const FunctionCallbackInfo<Value>& info) {
			try {
				Local<Value> result = ParameterizeHelperCtorStart<O>(info, F);
				if (result.IsEmpty()) {
					throw std::runtime_error("Member function returned empty Local<> but did not set exception");
				}
				info.GetReturnValue().Set(result);
			} catch (const js_error_base& err) {}
		}

		// Helper which converts member functions to `Type(Class* that, Args...)`
		template <typename F>
		struct MethodCast;

		template <typename R, typename ...Args>
		struct MethodCast<R(Args...)> {
			typedef R(*Type)(Args...);
			template <R(*F)(Args...)>
			static R Invoke(Args... args) {
				return (*F)(args...);
			}
		};

		template<class C, class R, class... Args>
		struct MethodCast<R(C::*)(Args...)> : public MethodCast<R(Args...)> {
			typedef R(*Type)(C*, Args...);
			template <R(C::*F)(Args...)>
			static R Invoke(C* that, Args... args) {
				return (that->*F)(args...);
			}
		};

		/**
		 * Convenience wrapper for the obtuse SetWeak function signature. When the callback is called
		 * the handle will already be gone.
		 */
		template <typename P, void (*F)(P*)>
		void SetWeak(P* param) {
			ShareableIsolate& isolate = ShareableIsolate::GetCurrent();
			isolate.AddWeakCallback(&this->handle, (void(*)(void*))F, param);
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
			ShareableIsolate& isolate = ShareableIsolate::GetCurrent();
			isolate.RemoveWeakCallback(&that->handle);
			delete that;
		}

		/**
		 * Transfer ownership of this C++ pointer to the v8 handle lifetime.
		 */
		static void Wrap(std::unique_ptr<ClassHandle> ptr, Local<Object> handle) {
			handle->SetAlignedPointerInInternalField(0, ptr.get());
			ptr->handle.Reset(Isolate::GetCurrent(), handle);
			ClassHandle* ptr_raw = ptr.release();
			ptr_raw->SetWeak<ClassHandle, WeakCallback>(ptr_raw);
		}

		/**
		 * It just throws when you call it; used when `nullptr` is passed as constructor
		 */
		static void PrivateConstructor(const FunctionCallbackInfo<Value>& info) {
			PrivateConstructorError(info);
		}

	protected:
		/**
		 * Sets up this object's FunctionTemplate inside the current isolate
		 */
		template <typename... Args>
		static Local<FunctionTemplate> MakeClass(const char* class_name, FunctionCallback New, int length, Args... args) {
			Isolate* isolate = Isolate::GetCurrent();
			Local<String> name_handle = v8_symbol(class_name);
			Local<FunctionTemplate> tmpl = FunctionTemplate::New(
				isolate, New == nullptr ? PrivateConstructor : New,
				name_handle, Local<Signature>(), length
			);
			tmpl->SetClassName(name_handle);
			tmpl->InstanceTemplate()->SetInternalFieldCount(1);

			Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
			Local<Signature> sig = Signature::New(isolate, tmpl);
			AddMethods(isolate, proto, sig, args...);

			return tmpl;
		}

		/**
		 * Inherit from another class's FunctionTemplate
		 */
		template <typename T>
		static Local<FunctionTemplate> Inherit(Local<FunctionTemplate> definition) {
			Local<FunctionTemplate> parent = GetFunctionTemplate<T>();
			definition->Inherit(parent);
			return definition;
		}

		/**
		 * Automatically unpacks v8 FunctionCallbackInfo for you
		 */
		template <typename T, T F>
		static inline void Parameterize(const FunctionCallbackInfo<Value>& info) {
			ParameterizeEntry<-1, typename MethodCast<T>::Type, MethodCast<T>::template Invoke<F>>(info);
		}

		template <typename T, T F>
		static inline void ParameterizeCtor(const FunctionCallbackInfo<Value>& info) {
			ParameterizeCtorEntry<0, T, F>(info);
		}

		template <typename T, T F>
		static inline void ParameterizeStatic(const FunctionCallbackInfo<Value>& info) {
			ParameterizeEntry<0, T, F>(info);
		}

	public:
		ClassHandle() = default;
		ClassHandle(const ClassHandle&) = delete;
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
			Local<Object> instance = Unmaybe(GetFunctionTemplate<T>()->InstanceTemplate()->NewInstance(Isolate::GetCurrent()->GetCurrentContext()));
			Wrap(std::make_unique<T>(std::forward<Args>(args)...), instance);
			return instance;
		}

		/**
		 * Pull out native pointer from v8 handle
		 */
		static ClassHandle* Unwrap(Local<Object> handle) {
			assert(!handle.IsEmpty());
			assert(handle->InternalFieldCount() > 0);
			return static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0));
		}

};

}
