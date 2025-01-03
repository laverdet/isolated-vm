module;
#include <string_view>
export module ivm.napi:string;
import :utility;
import :value;
import ivm.js;
import napi;

namespace ivm::js::napi {

// string constructors
template <>
struct factory<string_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		auto operator()(std::string_view string) const -> value<string_tag_of<std::string>>;
		auto operator()(std::u16string_view string) const -> value<string_tag_of<std::u16string>>;
		template <util::string_literal Literal>
		auto operator()(const js::string_literal<Literal>& literal) const -> value<string_tag_of<std::string>>;
};

template <>
struct factory<string_tag_of<std::string>> : factory<string_tag> {
		using factory<string_tag>::factory;
};

template <>
struct factory<string_tag_of<std::u16string>> : factory<string_tag> {
		using factory<string_tag>::factory;
};

template <util::string_literal Literal>
auto factory<string_tag>::operator()(const js::string_literal<Literal>& /*literal*/) const -> value<string_tag_of<std::string>> {
	return value<string_tag_of<std::string>>::from(js::napi::invoke(napi_create_string_latin1, env(), Literal.data(), Literal.length()));
}

} // namespace ivm::js::napi
