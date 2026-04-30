export module napi_js:value_handle;
import :handle.types;
import auto_js;
import nodejs;
import std;

namespace js::napi {

// Heirarchy:
// value_of<object_tag> ->
// value_for_object ->
// value_next<object_tag> ->
// value_of<value_tag> -> ...

// Details applied to each level of the `value_of<T>` hierarchy.
template <class Tag>
class value_next : public value_of<typename Tag::tag_type> {
	public:
		using value_of<typename Tag::tag_type>::value_of;

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value_of<To> { return value_of<To>::from(*this); }

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value_of<Tag> { return std::bit_cast<value_of<Tag>>(value_); }
};

// Tagged napi_value
export template <class Tag>
class value_of : public value_specialization<Tag>::value_type {
	public:
		using value_specialization<Tag>::value_type::value_type;
};

// Sentinel instantiation
template <>
class value_of<void> : public value_handle {
	public:
		using value_handle::value_handle;
};

} // namespace js::napi

// Specialize for `js::forward`. Makes `js::forward{value_of<Tag>}` infer
// `js::forward<Tag, ...>`.
namespace js {
template <class Tag>
struct forward_tag_for<napi::value_of<Tag>> : std::type_identity<Tag> {};
} // namespace js
