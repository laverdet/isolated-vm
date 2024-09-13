module;
#include <boost/variant.hpp>
#include <ranges>
#include <variant>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Type>
struct visit {
		constexpr auto operator()(const Type& value, const auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, value);
		}

		constexpr auto operator()(Type&& value, const auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, value);
		}
};

// `visit` delegation which invokes the visitor for the underlying type of a given value.
export template <class Type>
constexpr auto invoke_visit(Type&& value, auto&& accept) -> decltype(auto) {
	return visit<std::decay_t<Type>>{}(
		std::forward<decltype(value)>(value),
		std::forward<decltype(accept)>(accept)
	);
}

// Visiting a `boost::variant` visits the underlying member
template <class... Types>
struct visit<boost::variant<Types...>> {
		constexpr auto operator()(auto&& value, auto&& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ &accept ](auto&& value) constexpr {
					return invoke_visit(
						std::forward<decltype(value)>(value),
						std::forward<decltype(accept)>(accept)
					);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class... Types>
struct visit<std::variant<Types...>> {
		constexpr auto operator()(auto&& value, auto&& accept) const -> decltype(auto) {
			return std::visit(
				[ &accept ](auto&& value) constexpr {
					return invoke_visit(
						std::forward<decltype(value)>(value),
						std::forward<decltype(accept)>(accept)
					);
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

template <class Type, size_t Size>
struct visit<array_to_object_helper<Type, Size>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(dictionary_tag{}, std::forward<decltype(value)>(value));
		}
};

template <class Value, size_t Size>
struct visit<std::array<std::pair<std::string, Value>, Size>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(dictionary_tag{}, array_to_object_helper{std::forward<decltype(value)>(value)});
		}
};

} // namespace ivm::value
