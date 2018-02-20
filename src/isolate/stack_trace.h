#pragma once
#include <v8.h>
#include "class_handle.h"

namespace ivm {

/**
 * v8::StackTrace doesn't inherit from v8::Value so it can't be set as a property of an object which
 * is what we need to do to pass stack traces in an efficent way.  So this gets around that.
 */
class StackTraceHolder : public ClassHandle {
	public:
		v8::Persistent<v8::StackTrace, v8::CopyablePersistentTraits<v8::StackTrace>> stack_trace;
		explicit StackTraceHolder(v8::Local<v8::StackTrace> stack_handle);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		static v8::Local<v8::Value> AttachStack(v8::Local<v8::Value> error, v8::Local<v8::StackTrace> stack);
		static v8::Local<v8::Value> ChainStack(v8::Local<v8::Value> error, v8::Local<v8::StackTrace> stack);
};

} // namespace ivm
