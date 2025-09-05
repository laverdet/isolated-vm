module;
#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
export module isolated_js.dictionary.accept;
import isolated_js.dictionary.helpers;
import isolated_js.dictionary.vector_of;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// If the container is not recursive then it will own its own entry acceptor. Otherwise it accepts
// a reference to an existing one.
template <class Meta, class Type>
struct accept_entry_value : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept_entry_value<Meta, Type> {
	private:
		using accept_type = accept_next<Meta, Type>;

	public:
		explicit constexpr accept_entry_value(auto* previous) :
				accept_{previous} {}

		constexpr auto operator()(auto_tag auto tag, auto&&... args) const -> Type
			requires std::invocable<accept_type, decltype(tag), decltype(args)...> {
			return (*accept_)(tag, std::forward<decltype(args)>(args)...);
		}

	private:
		const accept_type* accept_;
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
		explicit constexpr accept_vector_value(auto* previous) :
				first{previous},
				second{previous} {}

		constexpr auto operator()(auto&& entry, const auto& visit) const -> std::pair<Key, Value> {
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
		using accept_vector_value<Meta, Entry>::accept_vector_value;

		constexpr auto operator()(Tag /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			const accept_vector_value<Meta, Entry>& accept_value = *this;
			return vector_of<Tag, Entry>{
				std::from_range,
				util::into_range(std::forward<decltype(dictionary)>(dictionary)) |
					std::views::transform([ & ](auto&& entry) -> Entry {
						return accept_value(std::forward<decltype(entry)>(entry), visit);
					})
			};
		}

		template <std::size_t Size>
			requires std::is_same_v<dictionary_tag, Tag>
		constexpr auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			const accept_vector_value<Meta, Entry>& accept_value = *this;
			// nb: The value category of `dictionary` is forwarded to *each* visitor. Move operations
			// should keep this in mind and only move one member at time.
			auto&& subject = accept_value.make_struct_subject(std::forward<decltype(dictionary)>(dictionary));
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					return vector_of<Tag, Entry>{std::in_place, invoke(std::integral_constant<size_t, Index>{})...};
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& visit_n = std::get<Index>(visit);
					return accept_value(std::forward<decltype(subject)>(subject), visit_n);
				},
				std::make_index_sequence<Size>{}
			);
		}
};

} // namespace js
