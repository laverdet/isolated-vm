export module auto_js:intrinsics.external;
import util;

namespace js {

// Discriminates a `tagged_external` alternative in a `std::variant` by its runtime type tag
template <class Type>
struct external_alternative {
		constexpr static auto discriminator = type<external_alternative>;
		constexpr static auto is_variant_discriminator = true;
};

// Holder for a runtime type-tagged external pointer.
export template <class Type>
class tagged_external : public util::pointer_facade {
	public:
		explicit tagged_external(Type& value) : value_{&value} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Type&() const { return *value_; }

		auto operator*() const -> Type& { return *value_; }

		constexpr static auto variant = external_alternative<Type>{};

	private:
		Type* value_;
};

} // namespace js
