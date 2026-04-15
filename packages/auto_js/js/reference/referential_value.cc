export module auto_js:referential_value;
import :reference_of;
import std;

namespace js {

// Filters `reference_of<T>` types from the given parameter pack.
constexpr auto reference_types_from = [](auto types) consteval {
	constexpr auto transform = util::overloaded{
		[]<class Type>(std::type_identity<reference_of<Type>> /*type*/) { return util::type_pack{type<Type>}; },
		[]<class Type>(std::type_identity<Type> /*type*/) { return util::type_pack{}; },
	};
	return util::pack_transform(types, transform);
};

// Recursive value type used by `referential_value`. This will probably be a `std::variant<T...>`.
// Values of `reference_of<T>` are stored outside this value in `storage_type`.
template <template <class> class Make, auto Extract>
class recursive_value_holder : public Make<recursive_value_holder<Make, Extract>> {
	public:
		using value_type = Make<recursive_value_holder<Make, Extract>>;
		using reference_types = type_t<reference_types_from(Extract(type<value_type>))>;
		using storage_type = type_t<util::spread_type_pack<reference_storage>(type<reference_types>)>;

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr recursive_value_holder(const value_type& value) : value_type{value} {}
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr recursive_value_holder(value_type&& value) : value_type{std::move(value)} {}
};

// Stores value and references corresponding to `recursive_value_holder`
export template <class Value, class Holder>
class referential_value : public util::pointer_facade {
	private:
		using storage_type = Holder::storage_type;

	public:
		using value_type = Value;

		// Initialize from a non-`reference_of<T>` value
		template <class Type>
		constexpr explicit referential_value(Type&& value)
			requires std::constructible_from<value_type, Type> :
				value_{std::forward<Type>(value)} {}

		// Initialize from a `reference_of<T>` value
		template <class Type>
		constexpr explicit referential_value(Type value)
			requires std::constructible_from<value_type, reference_of<Type>> :
				value_{reference_of<Type>{0}},
				references_{storage_type{std::move(value)}} {}

		// Initialize from value and reference storage payload
		constexpr explicit referential_value(value_type&& value, storage_type&& references) :
				value_{std::move(value)},
				references_{std::move(references)} {}

		[[nodiscard]] constexpr auto operator*() const -> const value_type& { return value_; }
		[[nodiscard]] constexpr auto operator*() && -> value_type&& { return std::move(value_); }
		[[nodiscard]] constexpr auto references() const& -> const storage_type& { return references_; }
		[[nodiscard]] constexpr auto references() && -> storage_type&& { return std::move(references_); }

	private:
		value_type value_;
		storage_type references_;
};

} // namespace js
