module;
#include <cassert>
#include <exception>
#include <expected>
#include <format>
#include <string>
#include <type_traits>
export module v8_js:error;
import :lock;
import :visit;
import isolated_js;
import v8;

namespace js::iv8 {

auto render_stack_trace(isolate_lock_witness lock, v8::Local<v8::StackTrace> stack_trace) -> std::u16string;

// Thrown from `unmaybe` to indicate that a v8 operation failed and there is an exception waiting on
// the JS stack.
export class pending_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending v8 error]"; }
};

// Transfer a v8 exception message into `js::error_value`, hopefully without throwing again.
auto transfer_error_value(isolate_lock_witness lock, v8::Local<v8::Message> message) -> js::error_value {
	auto stack_trace = message->GetStackTrace();
	return js::error_value{
		js::error::name_type::error,
		transfer_out_strict<std::u16string>(message->Get(), lock),
		stack_trace.IsEmpty()
			? u""
			: render_stack_trace(lock, stack_trace),
	};
}

// Unwrap a `MaybeLocal`, or throw `iv8::pending_error` on failure
export template <class Type>
auto unmaybe(v8::MaybeLocal<Type> maybe_value) -> v8::Local<Type> {
	v8::Local<Type> value;
	if (maybe_value.ToLocal(&value)) {
		return value;
	} else {
		throw iv8::pending_error{};
	}
}

// Set up try/catch block for use with `unmaybe`
export template <class Operation>
auto invoke_with_unmaybe(isolate_lock_witness lock, Operation operation) {
	const auto dispatch =
		[ & ]<class Type>(std::type_identity<Type> /*tag*/)
		-> std::expected<Type, js::error_value> {
		using value_type = std::expected<Type, js::error_value>;
		v8::TryCatch try_catch{lock.isolate()};
		try {
			return value_type{std::in_place, operation()};
		} catch (const iv8::pending_error& /*error*/) {
			assert(try_catch.HasCaught());
			return value_type{std::unexpect, transfer_error_value(lock, try_catch.Message())};
		}
	};
	return dispatch(std::type_identity<std::invoke_result_t<Operation>>{});
}

// Return `std::expected` from a single throwable v8 operation
export template <class Operation>
auto unmaybe_one(isolate_lock_witness lock, Operation operation) {
	const auto dispatch =
		[ & ]<class Type>(std::type_identity<v8::MaybeLocal<Type>> /*tag*/)
		-> std::expected<v8::Local<Type>, js::error_value> {
		using value_type = std::expected<v8::Local<Type>, js::error_value>;
		v8::TryCatch try_catch{lock.isolate()};
		v8::MaybeLocal<Type> maybe_value = operation();
		v8::Local<Type> value;
		if (maybe_value.ToLocal(&value)) {
			return value_type{std::in_place, value};
		} else {
			assert(try_catch.HasCaught());
			return value_type{std::unexpect, transfer_error_value(lock, try_catch.Message())};
		}
	};
	return dispatch(std::type_identity<std::invoke_result_t<Operation>>{});
}

// Render the stack trace in a similar manner as `Error.prototype.stack`
auto render_frame_line(v8::Local<v8::StackFrame> frame) -> std::u16string {
	if (auto line = frame->GetLineNumber(); line != -1) {
		if (auto column = frame->GetColumn(); column != -1) {
			return transfer_strict<std::u16string>(std::format("{}:{}", line, column));
		} else {
			return transfer_strict<std::u16string>(std::format("{}", line));
		}
	}
	return {};
}

// Render the stack trace in a similar manner as `Error.prototype.stack`
auto render_stack_trace(isolate_lock_witness lock, v8::Local<v8::StackTrace> stack_trace) -> std::u16string {
	std::u16string result;
	assert(stack_trace.IsEmpty());
	int size = stack_trace->GetFrameCount();
	for (int ii = 0; ii < size; ++ii) {
		result += ii == 0 ? u"    at " : u"\n    at ";

		auto frame = stack_trace->GetFrame(lock.isolate(), ii);
		auto fn_name = transfer_out_strict<std::u16string>(frame->GetFunctionName(), lock);
		auto script_name = transfer_out_strict<std::u16string>(frame->GetScriptName(), lock);
		auto has_script_name = !script_name.empty();
		auto has_fn_name = !fn_name.empty();
		bool has_name = has_fn_name || has_script_name;

		if (frame->IsWasm()) {
			if (has_name) {
				if (has_script_name) {
					result += script_name;
					if (has_fn_name) {
						result += u".";
						result += fn_name;
					}
				} else {
					result += fn_name;
				}
				result += u" (<WASM>";
			}
			result += render_frame_line(frame);
			if (has_name) {
				result += u")";
			}
		} else {
			if (frame->IsConstructor()) {
				result += u"new ";
			}
			result += has_fn_name ? fn_name : u"<anonymous>";
			result += u" (";
			if (has_script_name) {
				result += script_name;
			} else if (frame->IsEval()) {
				result += u"[eval]";
			} else {
				result += u"[unknown]";
			}
			result += render_frame_line(frame);
			result += u")";
		}
	}
	return result;
}

} // namespace js::iv8
