#pragma once
#include <v8.h>
#include "../apply_from_tuple.h"
#include "convert_param.h"
#include "environment.h"
#include "util.h"

#include <cassert>
#include <stdexcept>

namespace ivm {

/**
 * Analogous to node::ObjectWrap but Isolate-aware. Also has a bunch of unnecessary template stuff.
 */
class ClassHandle {
	private:
		v8::Persistent<v8::Object> handle;

		/**
		 * Utility methods to set up object prototype
		 */
		static void AddMethod(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& proto, v8::Local<v8::Signature>& sig, const char* name, v8::FunctionCallback impl, int length) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			proto->Set(name_handle, v8::FunctionTemplate::New(isolate, impl, name_handle, sig, length));
		}

		template <typename... Args>
		static void AddMethods(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& proto, v8::Local<v8::Signature>& sig, const char* name, v8::FunctionCallback impl, int length, Args... args) {
			AddMethod(isolate, proto, sig, name, impl, length);
			AddMethods(isolate, proto, sig, args...);
		}

		static void AddMethods(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& proto, v8::Local<v8::Signature>& sig) {
		}

		/**
		 * Below is the Parameterize<> template magic
		 */
		// These two templates will peel arguments off and run them through ConvertParam<T>
		template <int O, typename FnT, typename R, typename ConvertedArgsT>
		static inline R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, const v8::FunctionCallbackInfo<v8::Value>& info) {
			return apply_from_tuple(fn, convertedArgs);
		}

		template <int O, typename FnT, typename R, typename ConvertedArgsT, typename T, typename... ConversionArgsRest>
		static inline R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, const v8::FunctionCallbackInfo<v8::Value>& info) {
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
		static inline v8::Local<v8::Value> ParameterizeHelperStart(const v8::FunctionCallbackInfo<v8::Value>& info, v8::Local<v8::Value>(*fn)(Args...)) {
			return ParameterizeHelper<O, v8::Local<v8::Value>(*)(Args...), v8::Local<v8::Value>, std::tuple<>, Args...>(fn, std::tuple<>(), info);
		}

		// Constructor functions need to pair the C++ instance to the v8 handle
		template <int O, typename R, typename ...Args>
		static inline v8::Local<v8::Value> ParameterizeHelperCtorStart(const v8::FunctionCallbackInfo<v8::Value>& info, std::unique_ptr<R>(*fn)(Args...)) {
			RequireConstructorCall(info);
			std::unique_ptr<R> result = ParameterizeHelper<O, std::unique_ptr<R>(*)(Args...), std::unique_ptr<R>, std::tuple<>, Args...>(fn, std::tuple<>(), info);
			if (result.get() != nullptr) {
				v8::Local<v8::Object> handle = info.This().As<v8::Object>();
				Wrap(std::move(result), handle);
				return handle;
			} else {
				return Undefined(v8::Isolate::GetCurrent());
			}
		}

		// Main entry point for parameterized functions
		template <int O, typename T, T F>
		static inline void ParameterizeEntry(const v8::FunctionCallbackInfo<v8::Value>& info) {
			try {
				v8::Local<v8::Value> result = ParameterizeHelperStart<O>(info, F);
				if (result.IsEmpty()) {
					throw std::runtime_error("Member function returned empty Local<> but did not set exception");
				}
				info.GetReturnValue().Set(result);
			} catch (const js_runtime_error& err) {}
		}

		// Main entry point for parameterized constructors
		template <int O, typename T, T F>
		static inline void ParameterizeCtorEntry(const v8::FunctionCallbackInfo<v8::Value>& info) {
			try {
				v8::Local<v8::Value> result = ParameterizeHelperCtorStart<O>(info, F);
				if (result.IsEmpty()) {
					throw std::runtime_error("Member function returned empty Local<> but did not set exception");
				}
				info.GetReturnValue().Set(result);
			} catch (const js_runtime_error& err) {}
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
			auto isolate = IsolateEnvironment::GetCurrent();
			isolate->AddWeakCallback(&this->handle, (void(*)(void*))F, param);
			handle.SetWeak(param, WeakCallback<P, F>, v8::WeakCallbackType::kParameter);
		}
		template <typename P, void (*F)(P*)>
		static void WeakCallback(const v8::WeakCallbackInfo<P>& info) {
			F(info.GetParameter());
		}

		/**
		 * Invoked once JS loses all references to this object
		 */
		static void WeakCallback(ClassHandle* that) {
			auto isolate = IsolateEnvironment::GetCurrent();
			isolate->RemoveWeakCallback(&that->handle);
			delete that;
		}

		/**
		 * Transfer ownership of this C++ pointer to the v8 handle lifetime.
		 */
		static void Wrap(std::unique_ptr<ClassHandle> ptr, v8::Local<v8::Object> handle) {
			handle->SetAlignedPointerInInternalField(0, ptr.get());
			ptr->handle.Reset(v8::Isolate::GetCurrent(), handle);
			ClassHandle* ptr_raw = ptr.release();
			ptr_raw->SetWeak<ClassHandle, WeakCallback>(ptr_raw);
		}

		/**
		 * It just throws when you call it; used when `nullptr` is passed as constructor
		 */
		static void PrivateConstructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
			PrivateConstructorError(info);
		}

	protected:
		/**
		 * Sets up this object's FunctionTemplate inside the current isolate
		 */
		template <typename... Args>
		static v8::Local<v8::FunctionTemplate> MakeClass(const char* class_name, v8::FunctionCallback New, int length, Args... args) {
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			v8::Local<v8::String> name_handle = v8_symbol(class_name);
			v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
				isolate, New == nullptr ? PrivateConstructor : New,
				name_handle, v8::Local<v8::Signature>(), length
			);
			tmpl->SetClassName(name_handle);
			tmpl->InstanceTemplate()->SetInternalFieldCount(1);

			v8::Local<v8::ObjectTemplate> proto = tmpl->PrototypeTemplate();
			v8::Local<v8::Signature> sig = v8::Signature::New(isolate, tmpl);
			AddMethods(isolate, proto, sig, args...);

			return tmpl;
		}

		/**
		 * Inherit from another class's FunctionTemplate
		 */
		template <typename T>
		static v8::Local<v8::FunctionTemplate> Inherit(v8::Local<v8::FunctionTemplate> definition) {
			v8::Local<v8::FunctionTemplate> parent = GetFunctionTemplate<T>();
			definition->Inherit(parent);
			return definition;
		}

		/**
		 * Automatically unpacks v8 FunctionCallbackInfo for you
		 */
		template <typename T, T F>
		static inline void Parameterize(const v8::FunctionCallbackInfo<v8::Value>& info) {
			ParameterizeEntry<-1, typename MethodCast<T>::Type, MethodCast<T>::template Invoke<F>>(info);
		}

		template <typename T, T F>
		static inline void ParameterizeCtor(const v8::FunctionCallbackInfo<v8::Value>& info) {
			ParameterizeCtorEntry<0, T, F>(info);
		}

		template <typename T, T F>
		static inline void ParameterizeStatic(const v8::FunctionCallbackInfo<v8::Value>& info) {
			ParameterizeEntry<0, T, F>(info);
		}

		/*
		 * Utilities for non-method properties / statics
		 */
		static void AddProtoTemplate(v8::Local<v8::FunctionTemplate>& proto, const char* name, v8::Local<v8::FunctionTemplate> property) {
			proto->PrototypeTemplate()->Set(v8_symbol(name), property);
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
		static v8::Local<v8::Function> Init() {
			return GetFunctionTemplate<T>()->GetFunction();
		}

		/**
		 * Returns the FunctionTemplate for this isolate, generating it if needed.
		 */
		template <typename T>
		static v8::Local<v8::FunctionTemplate> GetFunctionTemplate() {
			IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& specific = T::TemplateSpecific();
			v8::MaybeLocal<v8::FunctionTemplate> maybe_tmpl = specific.Deref();
			v8::Local<v8::FunctionTemplate> tmpl;
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
		static v8::Local<v8::Object> NewInstance(Args... args) {
			v8::Local<v8::Object> instance = Unmaybe(GetFunctionTemplate<T>()->InstanceTemplate()->NewInstance(v8::Isolate::GetCurrent()->GetCurrentContext()));
			Wrap(std::make_unique<T>(std::forward<Args>(args)...), instance);
			return instance;
		}

		/**
		 * Pull out native pointer from v8 handle
		 */
		static ClassHandle* Unwrap(v8::Local<v8::Object> handle) {
			assert(!handle.IsEmpty());
			assert(handle->InternalFieldCount() > 0);
			return static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0));
		}

};

}
