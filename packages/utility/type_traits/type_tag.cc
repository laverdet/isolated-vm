export module util:type_traits.type_tag;
import std;

namespace util {

// RTTI-free `typeid` replacement.
export class type_tag {
	public:
		template <class Type>
		constexpr static auto of() -> type_tag {
			static_assert(!std::is_const_v<Type>);
			static_assert(!std::is_pointer_v<Type>);
			static_assert(!std::is_reference_v<Type>);
			static_assert(!std::is_volatile_v<Type>);
			return type_tag{&storage<Type>};
		}

		constexpr auto operator==(const type_tag&) const -> bool = default;

	private:
		constexpr explicit type_tag(const void* identity) : identity_{identity} {}

		template <class Type>
		constexpr static std::byte storage{};

		const void* identity_;
};

} // namespace util
