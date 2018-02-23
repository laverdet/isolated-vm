#include "stack_trace.h"
#include <cstring>
#include <sstream>
#include <string>

using namespace v8;

namespace ivm {

/**
 * This returns an object that's like Symbol() in JS but only C++ can see it.
 */
Local<Private> GetPrivateStackSymbol() {
	static IsolateEnvironment::IsolateSpecific<Private> holder;
	Local<Private> handle;
	if (!holder.Deref().ToLocal(&handle)) {
		handle = Private::New(Isolate::GetCurrent());
		holder.Set(handle);
	}
	return handle;
}

/**
 * Helper which renders to string either a String (pass-through), StackTrace (render), or Array
 * (recursion pair)
 */
Local<String> RenderErrorStack(Local<Value> data) {
	Isolate* isolate = Isolate::GetCurrent();
	if (data->IsString()) {
		// Plain string. We need to remove the first line of `stack` to avoid repeating the error
		// message
		String::Utf8Value string_value(data.As<String>());
		const char* c_str = *string_value;
		// Must not start with indentation
		if (c_str[0] == ' ' && c_str[1] == ' ' && c_str[2] == ' ' && c_str[3] == ' ') {
			return data.As<String>();
		}
		// Find the newline
		const char* newline = strchr(c_str, '\n');
		if (newline == nullptr) {
			return data.As<String>();
		}
		// Slice up to start of stack
		return Unmaybe(String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>(newline), NewStringType::kNormal));
	} else if (data->IsArray()) {
		// Array pair
		Local<Context> context = isolate->GetCurrentContext();
		Local<Array> array = data.As<Array>();
		return String::Concat(
			String::Concat(
				RenderErrorStack(Unmaybe(array->Get(context, 1))),
				Unmaybe(String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>("\n    at (<isolated-vm boundary>)"), NewStringType::kNormal))
			),
			RenderErrorStack(Unmaybe(array->Get(context, 0)))
		);
	} else {
		// StackTraceHolder
		StackTraceHolder& that = *dynamic_cast<StackTraceHolder*>(ClassHandle::Unwrap(data.As<Object>()));
		Local<StackTrace> stack_trace = Deref(that.stack_trace);
		std::stringstream ss;
		int size = stack_trace->GetFrameCount();
		for (int ii = 0; ii < size; ++ii) {
			Local<StackFrame> frame = stack_trace->GetFrame(ii);
			String::Utf8Value script_name(frame->GetScriptName());
			int line_number = frame->GetLineNumber();
			int column = frame->GetColumn();
			if (frame->IsEval()) {
				if (frame->GetScriptId() == Message::kNoScriptIdInfo) {
					ss <<"\n    at [eval]:" <<line_number <<":" <<column;
				} else {
					ss <<"\n    at [eval] (" <<*script_name <<":" <<line_number <<":" <<column;
				}
			} else {
				String::Utf8Value fn_name(frame->GetFunctionName());
				if (fn_name.length() == 0) {
					ss <<"\n    at " <<*script_name <<":" <<line_number <<":" <<column;
				} else {
					ss <<"\n    at " <<*fn_name <<" (" <<*script_name <<":" <<line_number <<":" <<column <<")";
				}
			}
		}
		return Unmaybe(String::NewFromUtf8(isolate, ss.str().c_str(), NewStringType::kNormal));
	}
}

/**
 * Accessor on error `stack`. Renders from previously saved stack trace.
 */
void ErrorStackGetter(Local<Name> property, const PropertyCallbackInfo<Value>& info) {
	try {
		Isolate* isolate = Isolate::GetCurrent();
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> holder = info.This();
		info.GetReturnValue().Set(String::Concat(
			String::Concat(holder->GetConstructorName(), v8_string(": ")),
			String::Concat(
				Unmaybe(
					Unmaybe(info.This()->Get(context, v8_string("message")))->ToString(context)
				),
				RenderErrorStack(Unmaybe(holder->GetPrivate(context, GetPrivateStackSymbol())))
			)
		));
	} catch (const js_runtime_error& cc_err) {
	} catch (const js_fatal_error& cc_err) {}
}

/**
 * Utility function which sets the stack getter on an error object
 */
void AttachStackGetter(Local<Object> error, Local<Value> data) {
	Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
	Unmaybe(error->SetPrivate(context, GetPrivateStackSymbol(), data));
	Unmaybe(error->SetAccessor(
		context,
		v8_string("stack"),
		ErrorStackGetter, nullptr,
		Local<Value>(),
		AccessControl::DEFAULT,
		PropertyAttribute::DontEnum
	));
}

/**
 * StackTraceHolder implementation
 */
StackTraceHolder::StackTraceHolder(Local<StackTrace> stack_handle) : stack_trace(Isolate::GetCurrent(), stack_handle) {}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& StackTraceHolder::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> StackTraceHolder::Definition() {
	return MakeClass("StackTraceHolder", nullptr);
}

Local<Value> StackTraceHolder::AttachStack(Local<Value> error, Local<StackTrace> stack) {
	return error;
	try {
		AttachStackGetter(error.As<Object>(), ClassHandle::NewInstance<StackTraceHolder>(stack));
	} catch (const js_runtime_error& cc_err) {
	} catch (const js_fatal_error& cc_err) {}
	return error;
}

Local<Value> StackTraceHolder::ChainStack(Local<Value> error, Local<StackTrace> stack) {
	return error;
	try {
		Isolate* isolate = Isolate::GetCurrent();
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> error_object = error.As<Object>();
		Local<Value> existing_data = Unmaybe(error_object->GetPrivate(context, GetPrivateStackSymbol()));
		if (existing_data->IsUndefined()) {
			// This error has not passed through ivm yet. Get the existing stack trace.
			Local<StackTrace> existing_stack = Exception::GetStackTrace(error);
			if (existing_stack.IsEmpty()) {
				// In this case it's probably passed through ExternalCopy which flattens the `stack` property
				// into a plain value
				existing_data = Unmaybe(error_object->Get(context, v8_string("stack")));
				if (existing_data->IsUndefined() || !existing_data->IsString()) {
					return AttachStack(error, stack);
				}
			} else {
				existing_data = ClassHandle::NewInstance<StackTraceHolder>(existing_stack);
			}
		}
		Local<Array> pair = Array::New(isolate, 2);
		Unmaybe(pair->Set(context, 0, ClassHandle::NewInstance<StackTraceHolder>(stack)));
		Unmaybe(pair->Set(context, 1, existing_data));
		AttachStackGetter(error_object, pair);
	} catch (const js_runtime_error& cc_err) {
	} catch (const js_fatal_error& cc_err) {}
	return error;
}

} // namespace ivm
