export module napi_js:value_handle;
import :handle.types;
import auto_js;
import nodejs;
import std;

namespace js::napi {

// Heirarchy:
// value<object_tag> ->
// value_for_object ->
// value_next<object_tag> ->
// value<value_tag> -> ...

// Details applied to each level of the `value<T>` hierarchy.
template <class Tag>
class value_next : public value<typename Tag::tag_type> {
	public:
		using value<typename Tag::tag_type>::value;

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> { return value<To>::from(*this); }

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value<Tag> { return std::bit_cast<value<Tag>>(value_); }
};

// Tagged napi_value
export template <class Tag>
class value : public value_specialization<Tag>::value_type {
	public:
		using value_specialization<Tag>::value_type::value_type;
};

// Sentinel instantiation
template <>
class value<void> : public value_handle {
	public:
		using value_handle::value_handle;
};

} // namespace js::napi

// Specialize for `js::forward`. Makes `js::forward{napi::value<Tag>}` infer
// `js::forward<Tag, ...>`.
namespace js {
template <class Tag>
struct js::forward_tag_for<napi::value<Tag>> : std::type_identity<Tag> {};
} // namespace js
