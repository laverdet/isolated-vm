module;
#include <concepts>
#include <functional>
#include <tuple>
export module ivm.utility:tuple;

namespace util {

// Construct a tuple with potentially non-moveable elements. It also fixes the annoying problem
// where `std::tuple{std::pair{1, 2}}` actually returns a `std::tuple<int, int>`, but that would be
// fine with `std::make_tuple`.
//
// You pass a packed argument of invocables which return the elements. The constructor for a tuple
// element cannot contain any single-argument `auto` constructors which could accept the underlying
// `construct` object. But who would do such a thing?
//
// Adapted from this solution into a generic utility:
// https://stackoverflow.com/questions/56187926/how-to-construct-a-tuple-of-non-movable-non-copyable-objects/56188636#56188636
namespace {
template <std::invocable<> Invocable>
struct construct_type {
		using result_type = std::invoke_result_t<Invocable>;
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator result_type() const { return invocable(); }
		Invocable invocable;
};
} // namespace

export template <std::invocable<>... Invocables>
constexpr auto make_tuple_in_place(Invocables... invocables) {
	return std::tuple<std::invoke_result_t<Invocables>...>{
		std::invoke([ & ]() constexpr {
			return construct_type<Invocables>{.invocable = std::move(invocables)};
		})...
	};
}

// Internal `flat_tuple` element storage
template <std::size_t Index, class Type>
class flat_tuple_element {
	public:
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
