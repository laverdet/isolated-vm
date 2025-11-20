module;
#include <type_traits>
export module util:type_traits.type_of;

// "ADL firewall"
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2538r0.html#solution
template <class Type>
struct hidden {
		struct of {
				using type = Type;
		};
};

template <class Type>
using hidden_t = hidden<Type>::of;

// Metaprogramming helper
// nb: Intentionally not exported, it should only be used via the `type` variable template.
template <class Hidden>
struct type_of;

// `util::type_of<Type>{}` alias
export template <class Type>
constexpr auto type = type_of<hidden_t<Type>>{};

// Extract the `type` of a value metatype
export template <auto Type>
using type_t = decltype(Type)::type;

// `type_of`
template <class Hidden>
struct type_of : std::type_identity<typename Hidden::type> {
		consteval type_of() = default;

		template <class Type>
			requires std::is_same_v<typename Type::type, typename Hidden::type>
		explicit consteval type_of(Type /*type*/){};

		consteval auto remove_cvref() const {
			return type<std::remove_cvref_t<typename Hidden::type>>;
		}

		// Equality operators
		consteval auto operator==(type_of /*op*/) const -> bool { return true; }

		template <class Op>
		consteval auto operator==(type_of<Op> /*op*/) const -> bool { return false; }

		template <class Op>
		consteval auto operator==(Op /*op*/) const -> bool { return std::is_same_v<typename Hidden::type, typename Op::type>; }

		// `true` represents a valid type
		consteval explicit operator bool() const { return true; }

		// Invoke result. Returns `std::false_type` if not invocable.
		consteval auto operator()(auto... types) const {
			if constexpr (requires { std::declval<typename Hidden::type>()(std::declval<type_t<types>>()...); }) {
				return type<decltype(std::declval<typename Hidden::type>()(std::declval<type_t<types>>()...))>;
			} else {
				return std::false_type{};
			}
		}
};
