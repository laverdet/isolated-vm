#pragma once
#include <v8.h>
#include "../apply_from_tuple.h"
#include "convert_param.h"
#include "environment.h"
#include "functor_runners.h"
#include "util.h"

#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace ivm {

/**
 * Analogous to node::ObjectWrap but Isolate-aware. Also has a bunch of unnecessary template stuff.
 */
class ClassHandle {
	private:
		struct CtorFunction {
			v8::FunctionCallback fn;
			int args;
			constexpr CtorFunction(v8::FunctionCallback fn, int args) : fn(fn), args(args) {}
			constexpr CtorFunction(std::nullptr_t ptr) : fn(ptr), args(0) {} // NOLINT
		};
		// The int types here are just a lazy way to make these different types
		using MemberFunction = std::pair<v8::FunctionCallback, int32_t>;
		using StaticFunction = std::pair<v8::FunctionCallback, uint32_t>;
		using AccessorPair = std::pair<v8::AccessorGetterCallback, v8::AccessorSetterCallback>;
		using StaticAccessorPair = std::pair<v8::FunctionCallback, v8::FunctionCallback>;

		using SetterParam = std::pair<v8::Local<v8::Value>*, const v8::PropertyCallbackInfo<void>*>;
		v8::Persistent<v8::Object> handle;

		/**
		 * Utility methods to set up object prototype
		 */
		// This add regular methods
		template <typename... Args>
		static void AddMethods(
			v8::Isolate* isolate,
			v8::Local<v8::FunctionTemplate>& tmpl,
			v8::Local<v8::ObjectTemplate>& proto,
			v8::Local<v8::Signature>& sig,
			v8::Local<v8::AccessorSignature>& asig,
			const char* name,
			MemberFunction impl,
			Args... args
		) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			proto->Set(name_handle, v8::FunctionTemplate::New(isolate, impl.first, name_handle, sig, impl.second));
			AddMethods(isolate, tmpl, proto, sig, asig, args...);
		}

		// This adds object templates to the prototype
		template <typename... Args>
		static void AddMethods(
			v8::Isolate* isolate,
			v8::Local<v8::FunctionTemplate>& tmpl,
			v8::Local<v8::ObjectTemplate>& proto,
			v8::Local<v8::Signature>& sig,
			v8::Local<v8::AccessorSignature>& asig,
			const char* name,
			v8::Local<v8::FunctionTemplate>& value,
			Args... args
		) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			proto->Set(name_handle, value);
			AddMethods(isolate, tmpl, proto, sig, asig, args...);
		}

		// This adds static functions on the object constructor
		template <typename... Args>
		static void AddMethods(
			v8::Isolate* isolate,
			v8::Local<v8::FunctionTemplate>& tmpl,
			v8::Local<v8::ObjectTemplate>& proto,
			v8::Local<v8::Signature>& sig,
			v8::Local<v8::AccessorSignature>& asig,
			const char* name,
			StaticFunction impl,
			Args... args
		) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			tmpl->Set(name_handle, v8::FunctionTemplate::New(isolate, impl.first, name_handle, v8::Local<v8::Signature>(), impl.second));
			AddMethods(isolate, tmpl, proto, sig, asig, args...);
		}

		// This adds accessors
		template <typename... Args>
		static void AddMethods(
			v8::Isolate* isolate,
			v8::Local<v8::FunctionTemplate>& tmpl,
			v8::Local<v8::ObjectTemplate>& proto,
			v8::Local<v8::Signature>& sig,
			v8::Local<v8::AccessorSignature>& asig,
			const char* name,
			AccessorPair impl,
			Args... args
		) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			proto->SetAccessor(name_handle, impl.first, impl.second, name_handle, v8::AccessControl::DEFAULT, v8::PropertyAttribute::None, asig);
			AddMethods(isolate, tmpl, proto, sig, asig, args...);
		}

		// This adds static accessors
		template <typename... Args>
		static void AddMethods(
			v8::Isolate* isolate,
			v8::Local<v8::FunctionTemplate>& tmpl,
			v8::Local<v8::ObjectTemplate>& proto,
			v8::Local<v8::Signature>& sig,
			v8::Local<v8::AccessorSignature>& asig,
			const char* name,
			StaticAccessorPair impl,
			Args... args
		) {
			v8::Local<v8::String> name_handle = v8_symbol(name);
			v8::Local<v8::FunctionTemplate> setter;
			if (impl.second != nullptr) {
				setter = v8::FunctionTemplate::New(isolate, impl.second, name_handle);
			}
			tmpl->SetAccessorProperty(name_handle, v8::FunctionTemplate::New(isolate, impl.first, name_handle), setter);
			AddMethods(isolate, tmpl, proto, sig, asig, args...);
		}

		static void AddMethods(v8::Isolate*, v8::Local<v8::FunctionTemplate>, v8::Local<v8::ObjectTemplate>&, v8::Local<v8::Signature>&, v8::Local<v8::AccessorSignature>&) {} // NOLINT

		/**
		 * Below is the Parameterize<> template magic
		 */
		// These two templates will peel arguments off and run them through ConvertParam<T>
		template <typename V, int O, typename FnT, typename R, typename ConvertedArgsT>
		static R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, V /* info */) {
			return apply_from_tuple(fn, convertedArgs);
		}

		template <typename V, int O, typename FnT, typename R, typename ConvertedArgsT, typename T, typename... ConversionArgsRest>
		static R ParameterizeHelper(FnT fn, ConvertedArgsT convertedArgs, V info) {
			auto t = std::tuple_cat(convertedArgs, std::make_tuple(
				ConvertParamInvoke<
					ConvertParam<T>,
					(int)(std::tuple_size<ConvertedArgsT>::value) + O,
					(int)(sizeof...(ConversionArgsRest) + std::tuple_size<ConvertedArgsT>::value) + O + 1
				>::Invoke(info)
			));
			return ParameterizeHelper<V, O, FnT, R, decltype(t), ConversionArgsRest...>(fn, t, info);
		}

		// Regular function that just returns a Local<Value>
		template <typename V, int O, typename ...Args>
		static v8::Local<v8::Value> ParameterizeHelperStart(V info, v8::Local<v8::Value>(*fn)(Args...)) {
			return ParameterizeHelper<V, O, v8::Local<v8::Value>(*)(Args...), v8::Local<v8::Value>, std::tuple<>, Args...>(fn, std::tuple<>(), info);
		}

		// Setter version (void return)
		template <typename V, int O, typename ...Args>
		static void ParameterizeHelperStart(V info, void(*fn)(Args...)) {
			return ParameterizeHelper<V, O, void(*)(Args...), void, std::tuple<>, Args...>(fn, std::tuple<>(), info);
		}

		// Constructor functions need to pair the C++ instance to the v8 handle
		template <int O, typename R, typename ...Args>
		static v8::Local<v8::Value> ParameterizeHelperCtorStart(const v8::FunctionCallbackInfo<v8::Value>& info, std::unique_ptr<R>(*fn)(Args...)) {
			RequireConstructorCall(info);
			std::unique_ptr<R> result = ParameterizeHelper<
				const v8::FunctionCallbackInfo<v8::Value>&,
				O,
				std::unique_ptr<R>(*)(Args...),
				std::unique_ptr<R>,
				std::tuple<>,
				Args...
			>(fn, std::tuple<>(), info);
			if (result) {
				v8::Local<v8::Object> handle = info.This().As<v8::Object>();
				Wrap(std::move(result), handle);
				return handle;
			} else {
				return Undefined(v8::Isolate::GetCurrent());
			}
		}

		// Main entry point for parameterized functions
		template <int O, typename T, T F>
		static void ParameterizeEntry(const v8::FunctionCallbackInfo<v8::Value>& info) {
			FunctorRunners::RunCallback(info, [ &info ]() {
				return ParameterizeHelperStart<const v8::FunctionCallbackInfo<v8::Value>&, O>(info, F);
			});
		}

		// Main entry point for parameterized constructors
		template <typename T, T F>
		static void ParameterizeCtorEntry(const v8::FunctionCallbackInfo<v8::Value>& info) {
			FunctorRunners::RunCallback(info, [ &info ]() {
				return ParameterizeHelperCtorStart<0>(info, F);
			});
		}

		// Main entry point for getter functions
		template <typename T, T F>
		static void ParameterizeGetterEntry(v8::Local<v8::String> /* property */, const v8::PropertyCallbackInfo<v8::Value>& info) {
			FunctorRunners::RunCallback(info, [ &info ]() {
				return ParameterizeHelperStart<const v8::PropertyCallbackInfo<v8::Value>&, -1>(info, F);
			});
		}

		// Main entry point for setter functions
		template <typename T, T F>
		static void ParameterizeSetterEntry(v8::Local<v8::String> /* property */, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) {
			FunctorRunners::RunCallback(info, [ &info, &value ]() {
				ParameterizeHelperStart<SetterParam, -1>(SetterParam(&value, &info), F);
				return v8::Boolean::New(v8::Isolate::GetCurrent(), true);
			});
		}

		// Helper which converts member functions to `Type(Class* that, Args...)`
		template <typename F>
		struct MethodCast;

		template <typename R, typename ...Args>
		struct MethodCast<R(Args...)> {
			using Type = R(*)(Args...);
			template <R(*F)(Args...)>
			static R Invoke(Args... args) {
				return (*F)(args...);
			}
		};

		template<class C, class R, class ...Args>
		struct MethodCast<R(C::*)(Args...)> : public MethodCast<R(Args...)> {
			using Type = R(*)(C*, Args...);
			template <R(C::*F)(Args...)>
			static R Invoke(C* that, Args... args) {
				return (that->*F)(args...);
			}
		};

		template <typename R, typename ...Args> static constexpr size_t ArgCount(R(* /* fn */)(Args...)) {
			return sizeof...(Args);
		}

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

		/**
		 * Returns a unique IsolateSpecific for each subclass
		 */
		template <typename T>
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate> tmpl;
			return tmpl;
		}

	protected:
		/**
		 * Sets up this object's FunctionTemplate inside the current isolate
		 */
		template <typename... Args>
		static v8::Local<v8::FunctionTemplate> MakeClass(const char* class_name, CtorFunction New, Args... args) {
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			v8::Local<v8::String> name_handle = v8_symbol(class_name);
			v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
				isolate, New.fn == nullptr ? PrivateConstructor : New.fn,
				name_handle, v8::Local<v8::Signature>(), New.args
			);
			tmpl->SetClassName(name_handle);
			tmpl->InstanceTemplate()->SetInternalFieldCount(1);

			v8::Local<v8::ObjectTemplate> proto = tmpl->PrototypeTemplate();
			v8::Local<v8::Signature> sig = v8::Signature::New(isolate, tmpl);
			v8::Local<v8::AccessorSignature> asig = v8::AccessorSignature::New(isolate, tmpl);
			AddMethods(isolate, tmpl, proto, sig, asig, args...);

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
		static constexpr MemberFunction Parameterize() {
			return MemberFunction(
				ParameterizeEntry<-1, typename MethodCast<T>::Type, MethodCast<T>::template Invoke<F>>,
				ArgCount(MethodCast<T>::template Invoke<F>) - 1
			);
		}

		template <typename T, T F>
		static constexpr CtorFunction ParameterizeCtor() {
			return CtorFunction(ParameterizeCtorEntry<T, F>, ArgCount(F));
		}

		template <typename T, T F>
		static constexpr StaticFunction ParameterizeStatic() {
			return StaticFunction(ParameterizeEntry<0, T, F>, ArgCount(F));
		}

		template <typename T1, T1 F1, typename T2, T2 F2>
		static constexpr AccessorPair ParameterizeAccessor() {
			return AccessorPair(
				ParameterizeGetterEntry<typename MethodCast<T1>::Type, MethodCast<T1>::template Invoke<F1>>,
				ParameterizeSetterEntry<typename MethodCast<T2>::Type, MethodCast<T2>::template Invoke<F2>>
			);
		}

		template <typename T1, T1 F1>
		static constexpr AccessorPair ParameterizeAccessor() {
			return AccessorPair(
				ParameterizeGetterEntry<typename MethodCast<T1>::Type, MethodCast<T1>::template Invoke<F1>>,
				(v8::AccessorSetterCallback)nullptr
			);
		}

		template <typename T1, T1 F1, typename T2, T2 F2>
		static constexpr StaticAccessorPair ParameterizeStaticAccessor() {
			return StaticAccessorPair(
				ParameterizeEntry<0, T1, F1>,
				ParameterizeEntry<0, T2, F2>
			);
		}

		template <typename T1, T1 F1>
		static constexpr StaticAccessorPair ParameterizeStaticAccessor() {
			return StaticAccessorPair(
				ParameterizeEntry<0, T1, F1>,
				nullptr
			);
		}

	public:
		ClassHandle() = default;
		ClassHandle(const ClassHandle&) = delete;
		ClassHandle& operator= (const ClassHandle&) = delete;
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
			IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& specific = TemplateSpecific<T>();
			v8::MaybeLocal<v8::FunctionTemplate> maybe_tmpl = specific.Deref();
			v8::Local<v8::FunctionTemplate> tmpl;
			if (maybe_tmpl.ToLocal(&tmpl)) {
				return tmpl;
			} else {
				tmpl = T::Definition();
				specific.Set(tmpl);
				return tmpl;
			}
		}

		/**
		 * Builds a new instance of T from scratch, used in factory functions.
		 */
		template <typename T, typename ...Args>
		static v8::Local<v8::Object> NewInstance(Args&&... args) {
			v8::Local<v8::Object> instance = Unmaybe(GetFunctionTemplate<T>()->InstanceTemplate()->NewInstance(v8::Isolate::GetCurrent()->GetCurrentContext()));
			Wrap(std::make_unique<T>(std::forward<Args>(args)...), instance);
			return instance;
		}

		/**
		 * Pull out native pointer from v8 handle
		 */
		template <typename T>
		static T* Unwrap(v8::Local<v8::Object> handle) {
			assert(!handle.IsEmpty());
			if (!ClassHandle::GetFunctionTemplate<T>()->HasInstance(handle)) {
				return nullptr;
			}
			assert(handle->InternalFieldCount() > 0);
			return dynamic_cast<T*>(static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0)));
		}

		static ClassHandle* UnwrapClassHandle(v8::Local<v8::Object> handle) {
			assert(!handle.IsEmpty());
			assert(handle->InternalFieldCount() > 0);
			return static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0));
		}

		/**
		 * Returns the JS value that this ClassHandle points to
		 */
		v8::Local<v8::Object> This() {
			return Deref(handle);
		}
};

} // namespace ivm
