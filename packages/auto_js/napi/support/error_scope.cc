export module napi_js:error_scope;
import :accept;
import :lock;

namespace js::napi {

// Catch `napi::pending_error` (thrown by `napi::invoke`), `napi::pending_v8_error` (thrown by
// `napi::unmaybe`), or `js::error`, converting to thrown runtime error.
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

} // namespace js::napi
