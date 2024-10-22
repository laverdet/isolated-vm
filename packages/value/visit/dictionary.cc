module;
#include <boost/variant.hpp>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
export module ivm.value:dictionary_visit;
import :dictionary;
import :visit;
import ivm.utility;

namespace ivm::value {

// Object key lookup for primitive dictionary variants. This should generally only be used for
// testing, since the subject is basically a C++ heap JSON payload.
template <class Meta, util::string_literal Key>
struct visit_key<Meta, Key, void> {
	public:
		constexpr visit_key(const auto_visit auto& visit) :
				first{visit} {}

		constexpr auto operator()(const auto& dictionary, const auto& visit, const auto_accept auto& accept) const {
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) {
				return visit.first(entry.first, first) == Key;
			});
			if (it != dictionary.end()) {
				return std::optional{visit.second(it->second, accept)};
			} else {
				return std::optional<std::decay_t<decltype(visit.second(it->second, accept))>>{};
			}
		}

	private:
		accept_next<Meta, std::string> first;
};

// Look for `boost::recursive_variant_` to determine if this container is recursive
template <class Type>
struct is_recursive : std::bool_constant<false> {};

template <class Type>
constexpr auto is_recursive_v = is_recursive<Type>::value;

template <>
struct is_recursive<boost::recursive_variant_> : std::bool_constant<true> {};

template <template <class...> class Type, class... Types>
struct is_recursive<Type<Types...>>
		: std::bool_constant<std::disjunction_v<is_recursive<Types>...>> {};

// Helper to instantiate a different `accept` or `visit` structure for recursive containers
template <class Type>
struct recursive;

// If the container is not recursive then it will own its own entry acceptor. Otherwise it accepts
// a reference to an existing one.
template <class Meta, class Type>
struct accept<Meta, recursive<Type>> : accept_next<Meta, Type> {
		constexpr accept(const auto_visit auto& visit) :
				accept_next<Meta, Type>{visit} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept*/) :
				accept{visit} {}
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept<Meta, recursive<Type>> {
	private:
		using accept_type = accept_next<Meta, Type>;

	public:
		// nb: No `auto_visit` constructor because this is the recursive case and we require a reference
		constexpr accept(int /*dummy*/, const auto_visit auto& /*visit*/, const auto_accept auto& accept) :
				accept_{&accept} {}

		constexpr auto operator()(auto_tag auto tag, auto&&... args) const -> decltype(auto)
			requires std::invocable<accept_type, decltype(tag), decltype(args)...> {
			return (*accept_)(tag, std::forward<decltype(args)>(args)...);
		}

	private:
		const accept_type* accept_;
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Key, class Value>
struct accept<Meta, dictionary<Tag, Key, Value>> {
	public:
		constexpr accept(const auto_visit auto& visit) :
				first{visit},
				second{visit} {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& accept) :
				first{dummy, visit, accept},
				second{dummy, visit, accept} {}

		constexpr auto operator()(Tag /*tag*/, auto&& value, const auto& visit) const -> dictionary<Tag, Key, Value> {
			return dictionary<Tag, Key, Value>{
				util::into_range(value) |
				std::views::transform([ & ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::pair{
						visit.first(std::forward<decltype(key)>(key), first),
						visit.second(std::forward<decltype(value)>(value), second)
					};
				})
			};
		}

	private:
		accept<Meta, recursive<Key>> first;
		accept<Meta, recursive<Value>> second;
};

// Non-recursive visitor
template <class Type>
struct visit<recursive<Type>> : visit<Type> {
		visit() = default;
		constexpr visit(int /*dummy*/, const auto& /*visit*/) {}
};

// Recursive visitor
template <class Type>
	requires is_recursive_v<Type>
struct visit<recursive<Type>> {
	public:
		visit() = delete;
		constexpr visit(int /*dummy*/, const auto_visit auto& visit) :
				visit_{&visit} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return (*visit_)(std::forward<decltype(value)>(value), accept);
		}

	private:
		const visit<Type>* visit_;
};

// Entrypoint for `dictionary`
template <class Tag, class Key, class Value>
struct visit<dictionary<Tag, Key, Value>> {
		visit() = default;
		constexpr visit(int dummy, const auto_visit auto& visit) :
				first{dummy, visit},
				second{dummy, visit} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value), *this);
		}

		visit<recursive<Key>> first;
		visit<recursive<Value>> second;
};

} // namespace ivm::value
