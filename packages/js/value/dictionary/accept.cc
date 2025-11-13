module;
#include <ranges>
#include <utility>
export module isolated_js:dictionary.accept;
import :dictionary.vector_of;
import :transfer;
import ivm.utility;

namespace js {

// Default acceptor for non-pair values (unused)
template <class Meta, class Type>
struct accept_vector_value;

// Special case for pairs
template <class Meta, class Key, class Value>
struct accept_vector_value<Meta, std::pair<Key, Value>> {
		explicit constexpr accept_vector_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		constexpr auto operator()(auto& visit, auto&& entry) const -> std::pair<Key, Value> {
			return std::pair{
				visit.first(std::forward<decltype(entry)>(entry).first, first),
				visit.second(std::forward<decltype(entry)>(entry).second, second),
			};
		}

		constexpr static auto make_struct_subject(auto&& entry) -> std::pair<std::nullptr_t, std::remove_cvref_t<decltype(entry)>> {
			// nb: `nullptr` is used here because this is a "bound" `visit_key_literal::visit` visitor
			// which simply returns a static string and does not need a value to visit.
			return {nullptr, std::forward<decltype(entry)>(entry)};
		}

		accept_value_recursive<Meta, Key> first;
		accept_value_recursive<Meta, Value> second;
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Entry>
struct accept<Meta, vector_of<Tag, Entry>> : accept_vector_value<Meta, Entry> {
		using accept_type = accept_vector_value<Meta, Entry>;
		using accept_type::accept_type;

		constexpr auto operator()(Tag /*tag*/, auto& visit, auto&& subject) const -> vector_of<Tag, Entry> {
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			return vector_of<Tag, Entry>{
				std::from_range,
				util::forward_range(std::forward<decltype(range)>(range)) |
					std::views::transform([ & ](auto&& entry) -> Entry {
						return util::invoke_as<accept_type>(*this, visit, std::forward<decltype(entry)>(entry));
					})
			};
		}

		template <std::size_t Size>
			requires(type<Tag> == type<dictionary_tag>)
		constexpr auto operator()(struct_tag<Size> /*tag*/, auto& visit, auto&& subject) const -> vector_of<Tag, Entry> {
			// nb: The value category of `subject` is forwarded to *each* visitor. Move operations should
			// keep this in mind and only move one member at time.
			auto&& value = accept_type::make_struct_subject(std::forward<decltype(subject)>(subject));
			const auto [... indices ] = util::sequence<Size>;
			return vector_of<Tag, Entry>{
				std::in_place,
				// NOLINTNEXTLINE(bugprone-use-after-move)
				util::invoke_as<accept_type>(*this, std::get<indices>(visit), std::forward<decltype(value)>(value))...,
			};
		}
};

} // namespace js
