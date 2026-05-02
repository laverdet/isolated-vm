export module isolated_vm:handle.value_of;
export import :handle.types;
import auto_js;
import std;

namespace isolated_vm {
using namespace js;

// Details applied to each level of the `value_of<T>` hierarchy.
template <class Tag>
class value_next : public value_of<typename Tag::tag_type> {
	public:
		using value_of<typename Tag::tag_type>::value_of;

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value_of<To> { return value_of<To>::from(*this); }

		// Construct from any `value_handle`. Potentially unsafe.
		static auto from(value_handle value_) -> value_of<Tag> { return std::bit_cast<value_of<Tag>>(value_); }
};

// Tagged isolated_vm value
export template <class Tag>
class value_of : public value_specialization<Tag>::value_type {
	public:
		using value_specialization<Tag>::value_type::value_type;
};

template <std::derived_from<bound_value<void>> Type>
value_of(Type) -> value_of<typename Type::tag_type>;

// Sentinel instantiation
template <>
class value_of<void> : public value_handle {
	public:
		using value_handle::value_handle;
};

} // namespace isolated_vm

// Specialize for `js::forward`. Makes `js::forward{value_of<Tag>}` infer
// `js::forward<Tag, ...>`.
namespace js {
template <class Tag>
struct forward_tag_for<isolated_vm::value_of<Tag>> : std::type_identity<Tag> {};
} // namespace js
