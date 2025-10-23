module;
#include <cassert>
#include <exception>
#include <expected>
#include <type_traits>
export module v8_js:error;
import :lock;
import isolated_js;
import v8;

namespace js::iv8 {

// Thrown from `unmaybe` to indicate that a v8 operation failed and there is an exception waiting on
// the JS stack.
export class pending_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending v8 error]"; }
};

// Transfer a v8 exception message into `js::error_value`, hopefully without throwing again.
auto transfer_error_value(v8::Local<v8::Message> message) -> js::error_value {
	return js::error_value{js::error::name_type::error, u"oh noo", {}};
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
		v8::TryCatch try_catch{lock.isolate()};
		try {
			return {std::in_place, operation()};
		} catch (const iv8::pending_error& /*error*/) {
			assert(try_catch.HasCaught());
			return {std::unexpect, transfer_error_value(try_catch.Message())};
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
			return value_type{std::unexpect, transfer_error_value(try_catch.Message())};
		}
	};
	return dispatch(std::type_identity<std::invoke_result_t<Operation>>{});
}

} // namespace js::iv8
