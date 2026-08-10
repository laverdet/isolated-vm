export module util:utility.sequence;
import :utility.constant_wrapper;
import std;

namespace util {

// Array value which can be destructured in constexpr
template <class Type, std::size_t Size>
struct constexpr_array {
		constexpr auto begin() -> Type* { return values; }
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		Type values[ Size ];
};

template <class Type>
struct constexpr_array<Type, 0> {
		constexpr auto begin() -> Type* { return nullptr; }
};

template <class... Types>
constexpr_array(Types...) -> constexpr_array<Types...[ 0 ], sizeof...(Types)>;

export template <std::size_t Index, class Type, std::size_t Size>
constexpr auto get(const constexpr_array<Type, Size>& array) -> Type {
	return array.values[ Index ];
}

// Return a sequence of `std::integral_constant`.
// Keep an eye on P1789 and then replace with `std::index_sequence_for`.
export template <std::size_t Size>
constexpr auto sequence_cw = [] consteval -> auto {
	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{util::cw<Index>...};
	}(std::make_index_sequence<Size>());
}();

// Return a sequence of constexpr indices
export template <std::size_t Size>
constexpr auto sequence = [] consteval -> auto {
#if defined(__clang__)
	return sequence_cw<Size>;
#else
	constexpr_array<std::size_t, Size> result{};
	std::ranges::copy(std::ranges::views::iota(std::size_t{0}, Size), result.begin());
	return result;
#endif
}();

} // namespace util

namespace std {
// `util::constexpr_array` destructuring specializations
template <std::size_t Index, class Type, std::size_t Size>
struct tuple_element<Index, util::constexpr_array<Type, Size>> {
		using type = Type;
};

template <class Type, std::size_t Size>
struct tuple_size<util::constexpr_array<Type, Size>> {
		constexpr static auto value = Size;
};
} // namespace std
