export module util:utility.array;
import std;

namespace util {

// Array value which can be destructured in constexpr
export template <class Type, std::size_t Size>
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
