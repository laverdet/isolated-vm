#pragma once
#include <v8.h>
#include "environment.h"
#include "functor_runners.h"
#include "util.h"
#include "generic/callbacks.h"
#include "generic/extract_params.h"
#include "generic/handle_cast.h"
#include "generic/read_option.h"

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <stdexcept>

namespace ivm {

namespace detail {
	struct ConstructorFunctionHolder : detail::FreeFunctionHolder {
		using FreeFunctionHolder::FreeFunctionHolder;
	};

	template <class Signature>
	struct ConstructorFunctionImpl;

	template <class Type>
	struct TemplateHolder {
		static IsolateSpecific<v8::FunctionTemplate> specific;
	};

	template <class Type>
	IsolateSpecific<v8::FunctionTemplate> TemplateHolder<Type>::specific;
}

/**
 * Analogous to node::ObjectWrap but Isolate-aware. Also has a bunch of unnecessary template stuff.
 */
class ClassHandle {
	template <class Signature>
	friend struct detail::ConstructorFunctionImpl;
	private:
		v8::Persistent<v8::Object> handle;

		/**
		 * Utility methods to set up object prototype
		 */
		struct TemplateDefinition {
			v8::Isolate* isolate;
			v8::Local<v8::FunctionTemplate>& tmpl;
			v8::Local<v8::ObjectTemplate>& proto;
			v8::Local<v8::Signature>& sig;
			v8::Local<v8::AccessorSignature>& asig;

			// This add regular methods
			template <typename... Args>
			void Add(const char* name, detail::MemberFunctionHolder impl, Args... args) {
				v8::Local<v8::String> name_handle = v8_symbol(name);
				proto->Set(name_handle, v8::FunctionTemplate::New(isolate, impl.callback, name_handle, sig, impl.length));
				Add(args...);
			}

			// This adds function templates to the prototype
			template <typename... Args>
			void Add(const char* name, v8::Local<v8::FunctionTemplate>& value, Args... args) {
				v8::Local<v8::String> name_handle = v8_symbol(name);
				proto->Set(name_handle, value);
				Add(args...);
			}

			// This adds static functions on the object constructor
			template <typename... Args>
			void Add(const char* name, detail::FreeFunctionHolder impl, Args... args) {
				v8::Local<v8::String> name_handle = v8_symbol(name);
				tmpl->Set(name_handle, v8::FunctionTemplate::New(isolate, impl.callback, name_handle, v8::Local<v8::Signature>(), impl.length));
				Add(args...);
			}

			// This adds accessors
			template <typename... Args>
			void Add(const char* name, detail::MemberAccessorHolder impl, Args... args) {
				v8::Local<v8::String> name_handle = v8_symbol(name);
				proto->SetAccessor(name_handle, impl.getter.callback, impl.setter.callback, name_handle, v8::AccessControl::DEFAULT, v8::PropertyAttribute::None, asig);
				Add(args...);
			}

			// This adds static accessors
			template <typename... Args>
			void Add(const char* name, detail::StaticAccessorHolder impl, Args... args) {
				v8::Local<v8::String> name_handle = v8_symbol(name);
				v8::Local<v8::FunctionTemplate> setter;
				if (impl.setter.callback != nullptr) {
					setter = v8::FunctionTemplate::New(isolate, impl.setter.callback, name_handle);
				}
				tmpl->SetAccessorProperty(name_handle, v8::FunctionTemplate::New(isolate, impl.getter.callback, name_handle), setter);
				Add(args...);
			}

			void Add() {} // NOLINT
		};

		/**
		 * Convenience wrapper for the obtuse SetWeak function signature. When the callback is called
		 * the handle will already be gone.
		 */
		template <typename P, void (*F)(P*)>
		void SetWeak(P* param) {
			auto* isolate = IsolateEnvironment::GetCurrent();
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
			auto* isolate = IsolateEnvironment::GetCurrent();
			isolate->RemoveWeakCallback(&that->handle);
			delete that; // NOLINT
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
			throw RuntimeTypeError(detail::CalleeName(info)+ " constructor is private");
		}

	protected:
		/**
		 * Sets up this object's FunctionTemplate inside the current isolate
		 */
		template <typename... Args>
		static auto MakeClass(const char* class_name, detail::ConstructorFunctionHolder New, Args... args) -> v8::Local<v8::FunctionTemplate> {
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			v8::Local<v8::String> name_handle = v8_symbol(class_name);
			v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
				isolate, New.callback == nullptr ? PrivateConstructor : New.callback,
				name_handle, {}, New.length
			);
			tmpl->SetClassName(name_handle);
			tmpl->InstanceTemplate()->SetInternalFieldCount(1);

			v8::Local<v8::ObjectTemplate> proto = tmpl->PrototypeTemplate();
			v8::Local<v8::Signature> sig = v8::Signature::New(isolate, tmpl);
			v8::Local<v8::AccessorSignature> asig = v8::AccessorSignature::New(isolate, tmpl);
			TemplateDefinition def{isolate, tmpl, proto, sig, asig};
			def.Add(args...);

			return tmpl;
		}

		/**
		 * Inherit from another class's FunctionTemplate
		 */
		template <typename T>
		static auto Inherit(v8::Local<v8::FunctionTemplate> definition) -> v8::Local<v8::FunctionTemplate> {
			v8::Local<v8::FunctionTemplate> parent = GetFunctionTemplate<T>();
			definition->Inherit(parent);
			return definition;
		}

	public:
		ClassHandle() = default;
		ClassHandle(const ClassHandle&) = delete;
		auto operator= (const ClassHandle&) -> ClassHandle& = delete;
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
		static auto Init() -> v8::Local<v8::Function> {
			return GetFunctionTemplate<T>()->GetFunction();
		}

		/**
		 * Returns the FunctionTemplate for this isolate, generating it if needed.
		 */
		template <class Type>
		static auto GetFunctionTemplate() -> v8::Local<v8::FunctionTemplate> {
			return detail::TemplateHolder<Type>::specific.Deref([&]() {
				return Type::Definition();
			});
		}

		/**
		 * Builds a new instance of T from scratch, used in factory functions.
		 */
		template <typename T, typename ...Args>
		static auto NewInstance(Args&&... args) -> v8::Local<v8::Object> {
			v8::Local<v8::Object> instance = Unmaybe(GetFunctionTemplate<T>()->InstanceTemplate()->NewInstance(v8::Isolate::GetCurrent()->GetCurrentContext()));
			Wrap(std::make_unique<T>(std::forward<Args>(args)...), instance);
			return instance;
		}

		/**
		 * Pull out native pointer from v8 handle
		 */
		template <typename T>
		static auto Unwrap(v8::Local<v8::Object> handle) -> T* {
			assert(!handle.IsEmpty());
			if (!ClassHandle::GetFunctionTemplate<T>()->HasInstance(handle)) {
				return nullptr;
			}
			assert(handle->InternalFieldCount() > 0);
			return dynamic_cast<T*>(static_cast<ClassHandle*>(handle->GetAlignedPointerFromInternalField(0)));
		}

		/**
		 * Returns the JS value that this ClassHandle points to
		 */
		auto This() -> v8::Local<v8::Object> {
			return Deref(handle);
		}
};

// Conversions from v8::Value -> Type&
template <class Type>
inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<Type&> /*tag*/) -> Type& {
	if (!value->IsObject()) {
		throw ParamIncorrect("an object");
	}
	v8::Local<v8::Object> handle = value.As<v8::Object>();
	if (handle->InternalFieldCount() != 1) {
		throw ParamIncorrect("something else");
	}
	auto* ptr = ClassHandle::Unwrap<Type>(handle);
	if (ptr == nullptr) {
		throw ParamIncorrect("something else");
	} else {
		return *ptr;
	}
}

namespace detail {

template <class Signature>
struct ConstructorFunctionImpl;

template <class Signature>
struct ConstructorFunctionImpl<Signature*> : ConstructorFunctionImpl<Signature> {};

template <class Return, class ...Args>
struct ConstructorFunctionImpl<Return(Args...)> {
	using Type = v8::Local<v8::Value>(*)(v8::Local<v8::Value> This, Args... args);

	template <Return(*Function)(Args...)>
	static inline auto Invoke(v8::Local<v8::Value> This, Args... args) -> v8::Local<v8::Value> {
		auto instance = Function(args...);
		if (instance) {
			v8::Local<v8::Object> handle = This.As<v8::Object>();
			ClassHandle::Wrap(std::move(instance), handle);
			return handle;
		} else {
			return Undefined(v8::Isolate::GetCurrent());
		}
	}
};

} // namespace detail

template <class Signature, Signature Function>
struct ConstructorFunction : detail::ConstructorFunctionHolder {
	using ConstructorFunctionHolder::ConstructorFunctionHolder;
	ConstructorFunction() : ConstructorFunctionHolder{
		Entry,
		std::tuple_size<typename detail::extract_arguments<Signature>::arguments>::value} {}

	static void Entry(const v8::FunctionCallbackInfo<v8::Value>& info) {
		detail::RunBarrier([&]() {
			if (!info.IsConstructCall()) {
				throw RuntimeTypeError(detail::CalleeName(info)+ " must be called with `new`");
			}
			ToCallback<-1, typename Impl::Type, &Impl::template Invoke<Function>>()(info);
		});
	}

	using Impl = detail::ConstructorFunctionImpl<Signature>;
};

} // namespace ivm
