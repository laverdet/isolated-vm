export module napi_js:error_scope;
import :accept;
import :api;
import :environment;

namespace js::napi {

// Catch `napi::pending_error` (thrown by `napi::invoke`) or `js::error`, converting to thrown
// runtime error.
constexpr auto invoke_napi_error_scope = [](environment& env, auto implementation) -> napi_value {
	try {
		return implementation();
	} catch (const napi::pending_error& /*error*/) {
		return napi_value{};
	} catch (const js::error& error) {
		auto* exception = js::transfer_in_strict<napi_value>(error, env);
		napi::invoke0(napi_throw, napi_env{env}, exception);
		return napi_value{};
	}
};

} // namespace js::napi
