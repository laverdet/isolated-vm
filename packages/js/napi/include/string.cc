module;
#include <cstddef>
#include <string_view>
#include <utility>
export module ivm.napi:string;
import :utility;
import ivm.utility;
import ivm.value;
import napi;

namespace ivm::js::napi {

export auto create_string(napi_env env, std::string_view value) -> napi_value;
export auto create_string(napi_env env, std::u16string_view value) -> napi_value;

export template <util::string_literal Value>
auto create_string_literal(napi_env env) -> napi_value {
	return js::napi::invoke(napi_create_string_latin1, env, Value.data(), Value.length());
}

} // namespace ivm::js::napi
