module;
#include <boost/variant.hpp>
#include <ranges>
#include <type_traits>
#include <utility>
export module ivm.value:dictionary_visit;
import :dictionary;
import :visit;

namespace ivm::value {

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
		accept() = default;
		constexpr accept(int /*dummy*/, const auto& /*accept*/) {}
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept<Meta, recursive<Type>> {
	private:
		using accept_type = accept_next<Meta, Type>;

	public:
		accept() = delete;
		constexpr accept(int /*dummy*/, const auto& accept) :
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
		accept() = default;
		constexpr accept(int dummy, const auto& accept) :
				first{dummy, accept},
				second{dummy, accept} {}

		auto operator()(Tag /*tag*/, auto&& value, const auto& visit) const -> dictionary<Tag, Key, Value> {
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
		constexpr visit(int /*dummy*/, const auto& visit) :
				visit_{&visit} {}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return (*visit_)(std::forward<decltype(value)>(value), accept);
		}

	private:
		const visit<Type>* visit_;
};

// Entrypoint for `dictionary`
template <class Tag, class Key, class Value>
struct visit<dictionary<Tag, Key, Value>> {
		visit() = default;
		constexpr visit(int dummy, const auto& visit) :
				first{dummy, visit},
				second{dummy, visit} {}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value), *this);
		}

		visit<recursive<Key>> first;
		visit<recursive<Value>> second;
};

} // namespace ivm::value
