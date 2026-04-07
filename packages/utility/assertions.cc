import std;
import util;

constexpr auto assert_strings_equal(auto... strings) {
	const auto [... left ] = std::tuple{std::basic_string_view{strings}...};
	const auto [... right ] = std::tuple{std::basic_string_view{strings}...};
	(..., ([ = ](auto left) -> void {
		 using char_type = decltype(left)::value_type;
		 // NOLINTNEXTLINE(modernize-type-traits)
		 if (((left != util::transcode_string<char_type>(right)) || ...)) {
			 std::unreachable();
		 }
	 }(left)));
}

[[maybe_unused]] constexpr auto result = []() -> int {
	// https://en.cppreference.com/w/cpp/language/string_literal.html

	// Quick check of interpolation matrix
	assert_strings_equal(u8"💖", u"💖");
	assert_strings_equal("wow", u8"wow", u"wow", U"wow");

	// Unpaired utf-16 surrogates
	constexpr auto one_high_surrogate = std::basic_string_view{u"\xd83d"};
	static_assert(one_high_surrogate == util::transcode_string<char16_t>(one_high_surrogate));
	constexpr auto two_high_surrogate = std::basic_string_view{u"\xd83d\xd83d"};
	static_assert(two_high_surrogate == util::transcode_string<char16_t>(two_high_surrogate));
	return 0;
}();
