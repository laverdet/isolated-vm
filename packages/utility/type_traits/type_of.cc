module;
#include <type_traits>
export module ivm.utility:type_traits.type_of;

namespace util {

// Metaprogramming helper
// nb: Intentionally not exported, it should only be used via the `type` variable template.
template <class Type>
struct type_of : std::type_identity<Type> {
		// equality
		consteval auto operator==(type_of /*op*/) const -> bool { return true; }

		template <class Op>
		consteval auto operator==(type_of<Op> /*op*/) const -> bool { return false; }

		template <class Op>
		consteval auto operator==(Op /*op*/) const -> bool { return *this == type_of<typename Op::type>{}; }
};

} // namespace util

// `util::type_of<Type>{}` alias
export template <class Type>
constexpr auto type = util::type_of<Type>{};

// Extract the `type` of a metaprogramming value
export template <auto Type>
using type_t = decltype(Type)::type;
