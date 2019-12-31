#pragma once
#include <v8.h>
#include <atomic>

namespace ivm {

/**
 * Like thread_local data, but specific to an Isolate instead.
 */
template <class Type>
class IsolateSpecific {
	template <class>
	friend class IsolateSpecific;

	public:
		IsolateSpecific() : key{IsolateSpecific<void>::size++} {}

		template <class Functor>
		auto Deref(Functor callback) -> v8::Local<Type>;

	private:
		size_t key;
		static std::atomic<size_t> size;
		union HandleConvert {
			explicit HandleConvert(v8::Local<v8::Data> data) : data{data} {}
			v8::Local<v8::Data> data;
			v8::Local<Type> value;
		};
};

template <class Type>
std::atomic<size_t> IsolateSpecific<Type>::size{0};

} // namespace ivm

#include "environment.h"
