#pragma once
#include <v8.h>
#include "class_handle.h"
#include <string>

namespace ivm {

/**
 * v8::StackTrace doesn't inherit from v8::Value so it can't be set as a property of an object which
 * is what we need to do to pass stack traces in an efficent way.  So this gets around that.
 */
class StackTraceHolder : public ClassHandle {
	private:
		static void AppendFileLocation(v8::Isolate* isolate, v8::Local<v8::StackFrame> frame, std::stringstream& ss);
	public:
		v8::Persistent<v8::StackTrace, v8::CopyablePersistentTraits<v8::StackTrace>> stack_trace;
		explicit StackTraceHolder(v8::Local<v8::StackTrace> stack_handle);
		static v8::Local<v8::FunctionTemplate> Definition();
		static void AttachStack(v8::Local<v8::Object> error, v8::Local<v8::StackTrace> stack);
		static void ChainStack(v8::Local<v8::Object> error, v8::Local<v8::StackTrace> stack);
		static std::string RenderSingleStack(v8::Local<v8::StackTrace> stack_trace);
};

} // namespace ivm
