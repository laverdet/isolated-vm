module;
#include <cassert>
#include <exception>
#include <expected>
#include <format>
#include <string>
#include <type_traits>
export module v8_js:error;
import :accept;
import :lock;
import :unmaybe;
import :visit;
import auto_js;
import v8;

namespace js::iv8 {

auto render_stack_trace(isolate_lock_witness lock, v8::Local<v8::StackTrace> stack_trace) -> std::u16string;

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

// Catch `iv8::pending_error` or `js::error`, converting to thrown runtime error.
auto invoke_with_error_scope(context_lock_witness lock, auto operation) {
	using value_type = std::optional<decltype(operation())>;
	try {
		return value_type{std::in_place, operation()};
	} catch (const iv8::pending_error& /*error*/) {
		return value_type{std::nullopt};
	} catch (const js::error& error) {
		auto exception = js::transfer_in_strict<v8::Local<v8::Value>>(error, lock);
		lock.isolate()->ThrowException(exception);
		return value_type{std::nullopt};
	}
};

// Set up try/catch block for use with `unmaybe`. `std::expected<T, js::error_value>` is returned.
export auto invoke_with_unmaybe(isolate_lock_witness lock, auto operation) {
	using value_type = std::expected<decltype(operation()), js::error_value>;
	const auto try_catch = v8::TryCatch{lock.isolate()};
	try {
		return value_type{std::in_place, operation()};
	} catch (const iv8::pending_error& /*error*/) {
		assert(try_catch.HasCaught());
		return value_type{std::unexpect, transfer_error_value(lock, try_catch.Message())};
	}
}

// Return `std::expected` from a single throwable v8 operation
export template <class Operation>
auto unmaybe_one(isolate_lock_witness lock, Operation operation) {
	const auto dispatch =
		[ & ]<class Type>(std::type_identity<v8::MaybeLocal<Type>> /*tag*/)
		-> std::expected<v8::Local<Type>, js::error_value> {
		using value_type = std::expected<v8::Local<Type>, js::error_value>;
		const auto try_catch = v8::TryCatch{lock.isolate()};
		auto maybe_value = v8::MaybeLocal<Type>{operation()};
		auto value = v8::Local<Type>{};
		if (maybe_value.ToLocal(&value)) {
			return value_type{std::in_place, value};
		} else {
			assert(try_catch.HasCaught());
			return value_type{std::unexpect, transfer_error_value(lock, try_catch.Message())};
		}
	};
	return dispatch(std::type_identity<std::invoke_result_t<Operation>>{});
}

export template <class Operation>
auto unmaybe_one(context_lock_witness lock, Operation operation) {
	return unmaybe_one(isolate_lock_witness{lock}, std::move(operation));
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
	const int size = stack_trace->GetFrameCount();
	for (int ii = 0; ii < size; ++ii) {
		result += ii == 0 ? u"    at " : u"\n    at ";

		auto frame = stack_trace->GetFrame(lock.isolate(), ii);
		auto fn_name = transfer_out_strict<std::u16string>(frame->GetFunctionName(), lock);
		auto script_name = transfer_out_strict<std::u16string>(frame->GetScriptName(), lock);
		auto has_script_name = !script_name.empty();
		auto has_fn_name = !fn_name.empty();
		const bool has_name = has_fn_name || has_script_name;

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
