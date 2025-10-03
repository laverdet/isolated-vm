module;
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
export module isolated_js:dictionary.accept;
import :dictionary.helpers;
import :dictionary.vector_of;
import :transfer;
import ivm.utility;

namespace js {

// If the container is not recursive then it will own its own entry acceptor. Otherwise it accepts
// a reference to an existing one.
template <class Meta, class Type>
struct accept_entry_value : accept_next<Meta, Type> {
		using accept_target_type = Type;
		using accept_next<Meta, Type>::accept_next;
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept_entry_value<Meta, Type> {
	public:
		using accept_target_type = Type;
		using accept_type = accept_next<Meta, Type>;

		explicit constexpr accept_entry_value(auto* transfer) :
				accept_{*transfer} {}

		constexpr auto operator()(auto_tag auto tag, const auto& visit, auto&& value) const -> Type
			requires std::invocable<accept_type&, decltype(tag), decltype(visit), decltype(value)> {
			return accept_(tag, visit, std::forward<decltype(value)>(value));
		}

	private:
		std::reference_wrapper<accept_type> accept_;
};

// Default acceptor for non-pair values
template <class Meta, class Type>
struct accept_vector_value : accept_entry_value<Meta, Type> {
		using accept_entry_value<Meta, Type>::accept_entry_value;

		constexpr auto make_struct_subject(auto&& entry) {
			return std::forward<decltype(entry)>(entry);
		}
};

// Special case for pairs
template <class Meta, class Key, class Value>
struct accept_vector_value<Meta, std::pair<Key, Value>> {
		explicit constexpr accept_vector_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		constexpr auto operator()(const auto& visit, auto&& entry) -> std::pair<Key, Value> {
			return std::pair{
				visit.first(std::forward<decltype(entry)>(entry).first, first),
				visit.second(std::forward<decltype(entry)>(entry).second, second),
			};
		}

		constexpr auto make_struct_subject(auto&& entry) const -> std::pair<std::nullptr_t, decltype(entry)> {
			// nb: `nullptr` is used here because this is a "bound" `visit_key_literal::visit` visitor
			// which simply returns a static string and does not need a value to visit.
			return {nullptr, std::forward<decltype(entry)>(entry)};
		}

		accept_next<Meta, Key> first;
		accept_entry_value<Meta, Value> second;
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Entry>
struct accept<Meta, vector_of<Tag, Entry>> : accept_vector_value<Meta, Entry> {
		using accept_type = accept_vector_value<Meta, Entry>;
		using accept_type::accept_type;

		constexpr auto operator()(Tag /*tag*/, const auto& visit, auto&& dictionary) -> vector_of<Tag, Entry> {
			return vector_of<Tag, Entry>{
				std::from_range,
				util::into_range(std::forward<decltype(dictionary)>(dictionary)) |
					std::views::transform([ & ](auto&& entry) -> Entry {
						return accept_type::operator()(visit, std::forward<decltype(entry)>(entry));
					})
			};
		}

		template <std::size_t Size>
			requires std::is_same_v<dictionary_tag, Tag>
		constexpr auto operator()(struct_tag<Size> /*tag*/, const auto& visit, auto&& dictionary) -> vector_of<Tag, Entry> {
			// nb: The value category of `dictionary` is forwarded to *each* visitor. Move operations
			// should keep this in mind and only move one member at time.
			auto&& subject = accept_type::make_struct_subject(std::forward<decltype(dictionary)>(dictionary));
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					return vector_of<Tag, Entry>{std::in_place, invoke(std::integral_constant<size_t, Index>{})...};
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& visit_n = std::get<Index>(visit);
					return accept_type::operator()(visit_n, std::forward<decltype(subject)>(subject));
				},
				std::make_index_sequence<Size>{}
			);
		}
};

} // namespace js
