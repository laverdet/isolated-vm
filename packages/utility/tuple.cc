module;
#include <type_traits>
#include <utility>
export module ivm.utility:tuple;

namespace util {

// Internal `flat_tuple` element storage
template <std::size_t Index, class Type>
class flat_tuple_element {
	public:
		flat_tuple_element() = default;
		constexpr explicit flat_tuple_element(Type&& value) :
				value_{std::move(value)} {}
		constexpr explicit flat_tuple_element(const Type& value) :
				value_{value} {}

		using index_type = std::integral_constant<std::size_t, Index>;
		constexpr auto get(index_type /*index*/) & -> Type& { return value_; }
		constexpr auto get(index_type /*index*/) && -> Type&& { return std::move(value_); }
		[[nodiscard]] constexpr auto get(index_type /*index*/) const& -> const Type& { return value_; }

	private:
		[[no_unique_address]] Type value_;
};

// Internal `flat_tuple` helper providing access into private `flat_tuple_element` via friend
// visibility
constexpr auto flat_tuple_get(auto index, auto&& tuple) -> auto&& {
	return std::forward<decltype(tuple)>(tuple).get(index);
}

// Internal `flat_tuple_storage` element collection storage
template <class Indices, class... Types>
class flat_tuple_storage;

template <std::size_t... Index, class... Type>
class flat_tuple_storage<std::index_sequence<Index...>, Type...> : private flat_tuple_element<Index, Type>... {
	public:
		flat_tuple_storage() = default;
		constexpr explicit flat_tuple_storage(auto&&... values) :
				flat_tuple_element<Index, Type>{std::forward<decltype(values)>(values)}... {}

	private:
		constexpr friend auto flat_tuple_get(auto, auto&&) -> auto&&;
		using flat_tuple_element<Index, Type>::get...;
};

// Simple replacement for `std::tuple` which is trivially copyable in the case its members are.
// See: `static_assert(!std::is_trivially_copyable_v<std::tuple<int>>);`
export template <class... Types>
class flat_tuple;

template <class... Types>
class flat_tuple : public flat_tuple_storage<std::make_index_sequence<sizeof...(Types)>, Types...> {
	public:
		using flat_tuple_storage<std::make_index_sequence<sizeof...(Types)>, Types...>::flat_tuple_storage;
};

// `std::get` replacement for `flat_tuple`
export template <std::size_t Index, class... Types>
constexpr auto get(flat_tuple<Types...>& tuple) -> auto& {
	return flat_tuple_get(std::integral_constant<std::size_t, Index>{}, tuple);
}

export template <std::size_t Index, class... Types>
constexpr auto get(flat_tuple<Types...>&& tuple) -> auto&& {
	return flat_tuple_get(std::integral_constant<std::size_t, Index>{}, std::move(tuple));
}

export template <std::size_t Index, class... Types>
constexpr auto get(const flat_tuple<Types...>& tuple) -> const auto& {
	return flat_tuple_get(std::integral_constant<std::size_t, Index>{}, tuple);
}

} // namespace util
