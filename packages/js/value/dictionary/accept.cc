module;
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
struct accept_entry_value : accept_like, accept_next<Meta, Type> {
		constexpr explicit accept_entry_value(auto accept_heritage) :
				accept_next<Meta, Type>{accept_heritage} {}
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept_entry_value<Meta, Type> : accept_like {
	private:
		using accept_type = accept_next<Meta, Type>;

	public:
		constexpr explicit accept_entry_value(auto accept_heritage) :
				accept_{&accept_heritage.accept} {}

		constexpr auto operator()(auto_tag auto tag, auto&&... args) const -> decltype(auto)
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
};

// Special case for pairs
template <class Meta, class Key, class Value>
struct accept_vector_value<Meta, std::pair<Key, Value>> {
		explicit constexpr accept_vector_value(auto accept_heritage) :
				first{accept_heritage},
				second{accept_heritage} {}

		accept_next<Meta, Key> first;
		accept_entry_value<Meta, Value> second;
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Entry>
struct accept<Meta, vector_of<Tag, Entry>> : accept_vector_value<Meta, Entry> {
		explicit constexpr accept(auto accept_heritage) :
				accept_vector_value<Meta, Entry>{accept_heritage} {}

		constexpr auto operator()(Tag /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			const accept_vector_value<Meta, Entry>& acceptor = *this;
			return vector_of<Tag, Entry>{
				util::into_range(std::forward<decltype(dictionary)>(dictionary)) |
				std::views::transform([ & ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::pair{
						visit.first(std::forward<decltype(key)>(key), acceptor.first),
						visit.second(std::forward<decltype(value)>(value), acceptor.second)
					};
				})
			};
		}

		template <std::size_t Size>
			requires std::is_same_v<dictionary_tag, Tag>
		constexpr auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			const accept_vector_value<Meta, Entry>& acceptor = *this;
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					return vector_of<Tag, Entry>{std::in_place, invoke(std::integral_constant<size_t, Index>{})...};
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& visit_n = std::get<Index>(visit);
					accept<void, accept_immediate<string_tag>> accept_string;
					return std::pair{
						visit_n.first(acceptor.first, accept_string),
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						visit_n.second(std::forward<decltype(dictionary)>(dictionary), acceptor.second),
					};
				},
				std::make_index_sequence<Size>{}
			);
		}
};

} // namespace js
