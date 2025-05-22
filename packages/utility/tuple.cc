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

} // namespace util
