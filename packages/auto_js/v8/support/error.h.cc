module;
#include <cassert>
export module v8_js:error;
import :accept;
import auto_js;
import std;
import v8;

namespace js::iv8 {

// Transfer a v8 exception message into `js::error_value`, hopefully without throwing again.
auto externalize_caught_error(context_lock_witness lock, const v8::TryCatch& try_catch) -> js::error_value;

// Transfer an arbitrary v8 value into `js::error_value`. Probably the result of a promise.
auto externalize_caught_error(context_lock_witness lock, v8::Local<v8::Value> value) -> js::error_value;

// Catch `iv8::pending_error` or `js::error`, converting to thrown v8 runtime error.
[[nodiscard]] auto invoke_internal_error_scope(context_lock_witness lock, auto operation) {
	using result_type = std::invoke_result_t<decltype(operation)>;
	using value_type = type_t<[]() {
		if constexpr (type<result_type> == type<void>) {
			return type<bool>;
		} else {
			return type<std::optional<result_type>>;
		}
	}()>;
	try {
		if constexpr (type<result_type> == type<void>) {
			operation();
			return true;
		} else {
			return value_type{operation()};
		}
	} catch (const iv8::pending_error& /*error*/) {
		return value_type{};
	} catch (const js::error& error) {
		auto exception = js::transfer_in_strict<v8::Local<v8::Value>>(error, lock);
		lock.isolate()->ThrowException(exception);
		return value_type{};
	}
};

// Catch pending js errors and externalize them as `js::error_value`. `std::expected<T,
// js::error_value>` is returned.
export [[nodiscard]] auto invoke_externalized_error_scope(context_lock_witness lock, auto operation) {
	using result_type = decltype(operation());
	using value_type = std::expected<result_type, js::error_value>;
	const auto try_catch = v8::TryCatch{lock.isolate()};
	try {
		if constexpr (type<result_type> == type<void>) {
			operation();
			return value_type{std::in_place};
		} else {
			return value_type{std::in_place, operation()};
		}
	} catch (const iv8::pending_error& /*error*/) {
		assert(try_catch.HasCaught());
		return value_type{std::unexpect, externalize_caught_error(lock, try_catch)};
	} catch (const js::error& error) {
		return value_type{std::unexpect, error};
	}
}

// Return `std::expected` from a single throwable v8 operation
export template <class Operation>
auto unmaybe_one(context_lock_witness lock, Operation operation) {
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
			return value_type{std::unexpect, externalize_caught_error(lock, try_catch)};
		}
	};
	return dispatch(std::type_identity<std::invoke_result_t<Operation>>{});
}

} // namespace js::iv8
