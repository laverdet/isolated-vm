module;
#include <algorithm>
#include <concepts>
#include <span>
#include <typeinfo>
export module isolated_js:external;

namespace js {

// v8 & napi only support tagging a type with a single tag. That wouldn't support polymorphic types
// though. This is a wrapper on top of that which maintains a span of compatible types.
export class tagged_external {
	private:
		using tag_span_type = std::span<const std::type_info* const>;

	protected:
		explicit constexpr tagged_external(tag_span_type tags) : tag_span_{tags} {}

	public:
		template <class Type>
		[[nodiscard]] constexpr auto contains(std::type_identity<Type> /*tag*/) const -> bool {
			constexpr auto projection = [](const std::type_info* info) -> const std::type_info& {
				return *info;
			};
			return std::ranges::contains(tag_span_, typeid(Type), projection);
		}

	private:
		tag_span_type tag_span_;
};

// NOTE: The `Rest...` feature (unused) is probably UB. `static_cast<tagged_external_of<T>>(...)` is
// used to cast matching types. But technically it wouldn't inherit from `tagged_external_of<T>`. It
// might be an issue or it might not. I think it would be fine since it doesn't affect layout.
export template <class Type, std::derived_from<Type>... Rest>
class tagged_external_of : public tagged_external, public Type {
	public:
		explicit constexpr tagged_external_of(auto&&... args)
			requires std::constructible_from<Type, decltype(args)...> :
				tagged_external{tags_},
				Type{std::forward<decltype(args)>(args)...} {}

	private:
		constexpr static auto tags_ = std::array{&typeid(Type), &typeid(Rest)...};
};

} // namespace js
