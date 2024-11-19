module;
#include <string_view>
module ivm.napi;
import :string;
import napi;

namespace ivm::napi {

auto create_string(napi_env env, std::string_view value) -> napi_value {
	return napi_invoke_checked(napi_create_string_latin1, env, value.data(), value.length());
}

auto create_string(napi_env env, std::u16string_view value) -> napi_value {
	return napi_invoke_checked(napi_create_string_utf16, env, value.data(), value.length());
}

} // namespace ivm::napi
