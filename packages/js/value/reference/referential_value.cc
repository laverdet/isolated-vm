module;
#include <concepts>
#include <type_traits>
#include <utility>
export module isolated_js:referential_value;
import :reference_of;

namespace js {

// Filters `reference_of<T>` types from the given parameter pack.
constexpr auto reference_types_from = [](auto types) consteval {
	constexpr auto transform = util::overloaded{
		[]<class Type>(std::type_identity<reference_of<Type>> /*type*/) { return util::type_pack{type<Type>}; },
		[]<class Type>(std::type_identity<Type> /*type*/) { return util::type_pack{}; },
	};
	return util::pack_transform(types, transform);
};

// Makes the `reference_storage` type.
constexpr auto reference_storage_type_for_values = [](auto types_pack) consteval {
	const auto [... types ] = types_pack;
	return type<reference_storage<type_t<types>...>>;
};

// Simple value holder implementing inheritable constructors and dereference operators.
template <class Type>
class in_place_value_holder : public util::pointer_facade {
	public:
		using value_type = Type;

		explicit constexpr in_place_value_holder(std::in_place_t /*tag*/, const Type& value)
			requires std::copy_constructible<value_type> :
				value_{value} {}
		explicit constexpr in_place_value_holder(std::in_place_t /*tag*/, Type&& value)
			requires std::move_constructible<value_type> :
				value_{std::move(value)} {}
		explicit constexpr in_place_value_holder(std::in_place_t /*tag*/, auto&&... args)
			requires std::constructible_from<value_type, decltype(args)...> :
				value_{std::forward<decltype(args)>(args)...} {}

		[[nodiscard]] constexpr auto operator*() const -> const value_type& { return value_; }
		[[nodiscard]] constexpr auto operator*() && -> value_type&& { return std::move(value_); }

	private:
		value_type value_;
};

// Recursive value type used by `referential_value`. The reference types used by this value types
// are stored within the type and made available to acceptors.
template <template <class> class Make, auto Extract>
class recursive_refs_value : public in_place_value_holder<Make<recursive_refs_value<Make, Extract>>> {
	private:
		using holder_type = in_place_value_holder<Make<recursive_refs_value<Make, Extract>>>;

	public:
		using holder_type::holder_type;
		using typename holder_type::value_type;
		using reference_types = type_t<reference_types_from(Extract(type<value_type>))>;
		constexpr static auto is_recursive_type = true;
};

// Stores value and references corresponding to `recursive_refs_value`
export template <template <class> class Make, auto Extract>
class referential_value : public recursive_refs_value<Make, Extract> {
	public:
		using value_type = recursive_refs_value<Make, Extract>;
		using typename value_type::reference_types;

	private:
		using reference_storage_type = type_t<reference_storage_type_for_values(reference_types{})>;

	public:
		// Initialize from a non-`reference_of<T>` value
		template <class Value>
		constexpr explicit referential_value(Value&& value)
			requires std::constructible_from<value_type, std::in_place_t, Value> :
				value_type{std::in_place, std::forward<Value>(value)} {}

		// Initialize from a `reference_of<T>` value
		template <class Value>
		constexpr explicit referential_value(Value value)
			requires std::constructible_from<value_type, std::in_place_t, reference_of<Value>> :
				value_type{std::in_place, reference_of<Value>{0}},
				references_{std::move(value)} {}

		// Initialize from value and reference storage payload
		constexpr explicit referential_value(value_type&& value, reference_storage_type&& references) :
				value_type{std::move(value)},
				references_{std::move(references)} {}

		[[nodiscard]] constexpr auto references() const -> const reference_storage_type& { return references_; }
		[[nodiscard]] constexpr auto references() && -> reference_storage_type&& { return std::move(references_); }

	private:
		reference_storage_type references_;
};

} // namespace js
