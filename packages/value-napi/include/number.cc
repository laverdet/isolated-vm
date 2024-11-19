module;
#include <cstdint>
export module ivm.napi:number;
import napi;

namespace ivm::napi {

export auto create_number(napi_env env, double value) -> napi_value;
export auto create_number(napi_env env, int32_t value) -> napi_value;
export auto create_number(napi_env env, int64_t value) -> napi_value;
export auto create_number(napi_env env, uint32_t value) -> napi_value;

export auto create_bigint(napi_env env, int64_t value) -> napi_value;
export auto create_bigint(napi_env env, uint64_t value) -> napi_value;

} // namespace ivm::napi
