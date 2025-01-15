module;
#include <string_view>
export module ivm.napi:string;
import :utility;
import :value;
import ivm.js;
import nodejs;

namespace js::napi {

// string constructors
template <>
struct factory<string_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		auto operator()(std::string_view string) const -> value<string_tag_of<char>>;
		auto operator()(std::u16string_view string) const -> value<string_tag_of<char16_t>>;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		auto operator()(const char string[ Size ]) -> value<string_tag_of<char>>;
};

template <>
struct factory<string_tag_of<char>> : factory<string_tag> {
		using factory<string_tag>::factory;
};

template <>
struct factory<string_tag_of<char16_t>> : factory<string_tag> {
		using factory<string_tag>::factory;
};

template <size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
auto factory<string_tag>::operator()(const char string[ Size ]) -> value<string_tag_of<char>> {
	return (*this)(std::string_view{string, Size});
}

} // namespace js::napi
