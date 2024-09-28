module;
#include <boost/variant.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <variant>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant {
		constexpr static bool value = true;
};

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Type>
struct visit;

// Base stateless visitor
template <>
struct visit<void> {
		visit() = default;
		// Copy constructors, or any constructors which begin with a reference to themselves, cannot be
		// inherited. The extra `int` allows this "copy constructor" to be inherited.
		constexpr visit(int /*ignore*/, const visit& that) :
				visit{that} {}
};

// Construct a new stateless or trivial visitor from an existing visitor
export template <class Type>
constexpr auto beget_visit(auto&& parent_visit) {
	return visit<std::decay_t<Type>>{0, std::forward<decltype(parent_visit)>(parent_visit)};
}

// `visit` delegation which invokes the visitor for the underlying type of a given value. Generally
// this should only be used for stateless or trivial visitors.
export template <class Type>
constexpr auto invoke_visit(const auto& parent_visitor, Type&& value, const auto& accept) -> decltype(auto) {
	return beget_visit<Type>(parent_visitor)(std::forward<decltype(value)>(value), accept);
}

// TODO: remove
export template <class Type>
constexpr auto invoke_visit(Type&& value, const auto& accept) -> decltype(auto) {
	return beget_visit<Type>(visit<void>{})(std::forward<decltype(value)>(value), accept);
}

// Tagged primitive types
template <class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct visit<Type> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, std::forward<decltype(value)>(value));
		}
};

// `std::optional` visitor may yield `undefined`
template <class Type>
struct visit<std::optional<Type>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			if (value) {
				return invoke_visit(*this, *std::forward<decltype(value)>(value), accept);
			} else {
				return accept(undefined_tag{}, std::monostate{});
			}
		}
};

// Visiting a `boost::variant` visits the underlying member
template <class... Types>
struct visit<boost::variant<Types...>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ](auto&& value) constexpr {
					return invoke_visit(*this, std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class... Types>
	requires is_variant<Types...>::value
struct visit<std::variant<Types...>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return std::visit(
				[ & ](auto&& value) constexpr {
					return invoke_visit(*this, std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// Array of pairs is a dictionary for testing
template <class Type, size_t Size>
struct array_to_object_helper {
		using container_type = std::array<std::pair<std::string, Type>, Size>;
		constexpr explicit array_to_object_helper(container_type values) :
				values{std::move(values)} {}

		constexpr auto begin() const { return values.begin(); }
		constexpr auto end() const { return values.end(); }
		constexpr auto get(std::string_view string) {
			auto it = std::ranges::find_if(values, [ string ](auto&& entry) constexpr {
				return entry.first == string;
			});
			if (it == values.end()) {
				throw std::logic_error("Key not found");
			}
			return it->second;
		}
		std::array<std::pair<std::string, Type>, Size> values;
};

template <class Value, size_t Size>
struct visit<std::array<std::pair<std::string, Value>, Size>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(dictionary_tag{}, array_to_object_helper{std::forward<decltype(value)>(value)});
		}
};

} // namespace ivm::value
