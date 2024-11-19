module;
#include <cstdint>
module ivm.napi;
import :number;
import :utility;
import napi;

namespace ivm::napi {

auto create_number(napi_env env, double value) -> napi_value {
	return napi_invoke_checked(napi_create_double, env, value);
}

auto create_number(napi_env env, int32_t value) -> napi_value {
	return napi_invoke_checked(napi_create_int32, env, value);
}

auto create_number(napi_env env, int64_t value) -> napi_value {
	return napi_invoke_checked(napi_create_int64, env, value);
}

auto create_number(napi_env env, uint32_t value) -> napi_value {
	return napi_invoke_checked(napi_create_uint32, env, value);
}

auto create_bigint(napi_env env, int64_t value) -> napi_value {
	return napi_invoke_checked(napi_create_bigint_int64, env, value);
}

auto create_bigint(napi_env env, uint64_t value) -> napi_value {
	return napi_invoke_checked(napi_create_bigint_uint64, env, value);
}

} // namespace ivm::napi
