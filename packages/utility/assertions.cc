#include <tuple> // IWYU pragma: keep
#include <type_traits>
import util;

constexpr auto assert_strings_equal(auto... strings) {
	const auto [... left ] = util::sequence<sizeof...(strings)>;
	const auto [... right ] = util::sequence<sizeof...(strings)>;
	static_assert(
		(([ = ]() -> bool {
			 auto left_string = strings...[ left ];
			 using char_type = std::remove_extent_t<typename decltype(left_string)::value_type>;
			 // NOLINTNEXTLINE(modernize-type-traits)
			 return ((left_string == util::interpolate_string<char_type>(strings...[ right ])) && ...);
		 }()) &&
		 ...)
	);
}

[[maybe_unused]] constexpr auto result = []() -> int {
	// https://en.cppreference.com/w/cpp/language/string_literal.html

	// Quick check of interpolation matrix
	assert_strings_equal(util::cw<u8"💖">, util::cw<u"💖">, util::cw<U"💖">);
	assert_strings_equal(util::cw<"wow">, util::cw<u8"wow">, util::cw<u"wow">, util::cw<U"wow">);

	// Unpaired utf-16 surrogates
	constexpr auto one_high_surrogate = util::cw<u"\xd83d">;
	static_assert(one_high_surrogate == util::interpolate_string<char16_t>(one_high_surrogate));
	constexpr auto two_high_surrogate = util::cw<u"\xd83d\xd83d">;
	static_assert(two_high_surrogate == util::interpolate_string<char16_t>(two_high_surrogate));
	return 0;
}();
