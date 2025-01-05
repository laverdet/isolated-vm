module;
#include <string_view>
module ivm.napi;
import :string;
import ivm.js;
import napi;

namespace ivm::js::napi {

auto factory<string_tag>::operator()(std::string_view string) const -> value<string_tag_of<char>> {
	return value<string_tag_of<char>>::from(js::napi::invoke(napi_create_string_latin1, env(), string.data(), string.length()));
}

auto factory<string_tag>::operator()(std::u16string_view string) const -> value<string_tag_of<char16_t>> {
	return value<string_tag_of<char16_t>>::from(js::napi::invoke(napi_create_string_utf16, env(), string.data(), string.length()));
}

} // namespace ivm::js::napi
