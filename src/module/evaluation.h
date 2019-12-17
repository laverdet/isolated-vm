#pragma once
#include "../isolate/generic/error.h"
#include "../isolate/generic/handle_cast.h"
#include <string>
#include <v8.h>

namespace ivm {

/**
 * Parses script origin information from an option object and returns a non-v8 holder for the
 * information which can then be converted to a ScriptOrigin, perhaps in a different isolate from
 * the one it was read in.
 */
class ScriptOriginHolder {
	public:
		explicit ScriptOriginHolder(v8::MaybeLocal<v8::Object> maybe_options, bool is_module = false);
		explicit operator v8::ScriptOrigin() const;

	private:
		std::string filename = "<isolated-vm>";
		int32_t column_offset = 0;
		int32_t line_offset = 0;
		bool is_module;
};

/**
 * Run a lambda which invokes the v8 compiler and annotate the exception with source / line number
 * if it throws.
 */
template <class Function>
decltype(auto) RunWithAnnotatedErrors(Function fn) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::TryCatch try_catch{isolate};
	try {
		return fn();
	} catch (const detail::RuntimeErrorWithMessage& cc_error) {
		throw std::logic_error("Invalid error thrown by RunWithAnnotatedErrors");
	} catch (const RuntimeError& cc_error) {
		try {
			assert(try_catch.HasCaught());
			v8::Local<v8::Context> context = isolate->GetCurrentContext();
			v8::Local<v8::Value> error = try_catch.Exception();
			v8::Local<v8::Message> message = try_catch.Message();
			assert(error->IsObject());
			int linenum = Unmaybe(message->GetLineNumber(context));
			int start_column = Unmaybe(message->GetStartColumn(context));
			std::string decorator =
				HandleCast<std::string>(message->GetScriptResourceName())+
				":" + std::to_string(linenum) +
				":" + std::to_string(start_column + 1);
			auto message_key = HandleCast<v8::Local<v8::String>>("message");
			std::string message_str = HandleCast<std::string>(Unmaybe(error.As<v8::Object>()->Get(context, message_key)));
			Unmaybe(error.As<v8::Object>()->Set(context, message_key, HandleCast<v8::Local<v8::String>>(message_str + " [" + decorator + "]")));
			isolate->ThrowException(error);
			throw RuntimeError();
		} catch (const RuntimeError& cc_error) {
			try_catch.ReThrow();
			throw RuntimeError();
		}
	}
}

} // namespace ivm
