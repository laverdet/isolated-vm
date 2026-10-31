export module napi_js:error;
import :accept;
import :api;
import :lock;
import :visit;
import auto_js;
import std;
import util;

namespace js::napi {

// Transfer an arbitrary napi value into `js::error_value`, hopefully without throwing again.
export template <class Environment>
auto externalize_caught_error(const environment_lock_witness_of<Environment>& lock, napi_value value) -> js::error_value;

// Catch `napi::pending_error` (thrown by `napi::invoke`), `napi::pending_v8_error` (thrown by `napi::unmaybe`), or
// `js::error`, converting to thrown runtime error.
export template <class Environment>
[[nodiscard]] auto invoke_internal_error_scope(const environment_lock_witness_of<Environment>& lock, auto implementation) -> napi_value {
	auto try_catch = environment_try_catch{lock};
	try {
		return implementation();
	} catch (const napi::pending_error& /*error*/) {
		return napi_value{};
	} catch (const napi::pending_v8_error& /*error*/) {
		std::ignore = napi::invoke0_maybe(napi_throw, napi_env{lock}, try_catch.take_exception());
		return napi_value{};
	} catch (const js::error& error) {
		auto* exception = js::transfer_in_strict<napi_value>(error, lock);
		napi::invoke0(napi_throw, napi_env{lock}, exception);
		return napi_value{};
	}
};

// Catch pending errors and externalize them as `js::error_value`. `std::expected<T, js::error_value>` is returned.
export template <class Environment>
[[nodiscard]] auto invoke_externalized_error_scope(const environment_lock_witness_of<Environment>& lock, auto operation) {
	using result_type = decltype(operation());
	using value_type = std::expected<result_type, js::error_value>;
	auto try_catch = environment_try_catch{lock};
	try {
		if constexpr (type<result_type> == type<void>) {
			operation();
			return value_type{std::in_place};
		} else {
			return value_type{std::in_place, operation()};
		}
	} catch (const napi::pending_error& /*error*/) {
		auto* exception = napi::invoke(napi_get_and_clear_last_exception, napi_env{lock});
		return value_type{std::unexpect, externalize_caught_error(lock, exception)};
	} catch (const napi::pending_v8_error& /*error*/) {
		return value_type{std::unexpect, externalize_caught_error(lock, try_catch.take_exception())};
	} catch (const js::error& error) {
		return value_type{std::unexpect, error};
	}
}

// ---

template <class Environment>
auto take_maybe_string(const environment_lock_witness_of<Environment>& lock, napi_value value) -> std::u16string {
	if (napi::invoke(napi_typeof, napi_env{lock}, value) == napi_string) {
		return transfer_out_strict<std::u16string>(local_of<string_tag>::from(value), lock);
	} else {
		return {};
	}
}

template <class Environment>
auto externalize_caught_error(const environment_lock_witness_of<Environment>& lock, napi_value value) -> js::error_value {
	auto* env = napi_env{lock};
	if (napi::invoke(napi_typeof, env, value) == napi_object) {
		const auto message_key = js::transfer_in_strict<napi_value>(util::cw<"message">, lock);
		const auto stack_key = js::transfer_in_strict<napi_value>(util::cw<"stack">, lock);
		auto message = take_maybe_string(lock, napi::invoke(napi_get_property, env, value, message_key));
		auto stack = take_maybe_string(lock, napi::invoke(napi_get_property, env, value, stack_key));
		return js::error_value{js::error_name::error, std::move(message), std::move(stack)};
	} else {
		// nb: `napi_coerce_to_string` reports `napi_string_expected`, instead of
		// `napi_pending_exception`, when the coercion throws.
		napi_value string{};
		if (napi_coerce_to_string(env, value, &string) == napi_ok) {
			return js::error_value{js::error_name::error, take_maybe_string(lock, string)};
		} else {
			std::ignore = napi::invoke(napi_get_and_clear_last_exception, env);
			return js::error_value{js::error_name::error, {}};
		}
	}
}

} // namespace js::napi
