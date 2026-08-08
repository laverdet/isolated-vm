export module napi_js:handle.local_of;
import :handle.types;
import auto_js;
import nodejs;
import std;
import util;

namespace js::napi {

// Map of `local_of<Tag>` to its concrete implementation class
template <class Tag>
struct local_specialization;

// Tagged napi_value. Each `local_of<T>` inherits from the tag before it which makes overloading
// acceptor functions naturally hierarchical.
export template <class Tag>
class local_of : public local_of<typename Tag::tag_type> {
	protected:
		using local_type = local_specialization<Tag>::type;
		explicit local_of(napi_value value) : local_of<typename Tag::tag_type>{value} {}

	public:
		local_of() = default;
		using tag_type = Tag;

		// Constructor from tagged `runtime_handle` flavors
		explicit local_of(local_type value)
			requires(type<local_type> != type<runtime_handle>) :
				local_of{napi_value{value}} {}

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> local_of<To> { return local_of<To>::from(*this); }

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> local_of<Tag> { return std::bit_cast<local_of<Tag>>(value_); }

		// Forward `make` to specialization
		static auto make(auto&&... args) -> local_of<Tag> { return local_type::make(std::forward<decltype(args)>(args)...); }

		// Dereference operators
		auto operator*() const -> local_type { return std::bit_cast<local_type>(napi_value{*this}); }
		auto operator->() const -> auto { return util::pointer_delegate{**this}; }
};

// Sentinel instantiation
template <>
class local_of<void> : public runtime_handle {
	public:
		using runtime_handle::runtime_handle;
};

// Deduction guide
template <class Type>
local_of(Type value) -> local_of<typename Type::tag_type>;

// Details applied to each level of the `local_of<T>` hierarchy.
template <class Tag>
struct local_next : local_specialization<typename Tag::tag_type>::type {
		using local_specialization<typename Tag::tag_type>::type::type;
		using tag_type = Tag;
};

// `local_of<Tag>` implementation specializations. The default specialization selects the nearest
// specialization, or simply `runtime_handle` as the base class.
template <class Type> class local_for_class_of;
template <class Type> class local_for_typed_array_of;

template <class Tag>
struct local_specialization
		: local_specialization<typename Tag::tag_type> {};

template <>
struct local_specialization<void>
		: std::type_identity<runtime_handle> {};

template <>
struct local_specialization<object_tag>
		: std::type_identity<class local_for_object> {};

template <>
struct local_specialization<function_tag>
		: std::type_identity<class local_for_function> {};

template <>
struct local_specialization<typed_array_tag>
		: std::type_identity<class local_for_typed_array> {};

template <class Type>
struct local_specialization<typed_array_tag_of<Type>>
		: std::type_identity<class local_for_typed_array_of<Type>> {};

template <>
struct local_specialization<data_view_tag>
		: std::type_identity<class local_for_data_view> {};

template <class Type>
struct local_specialization<class_tag_of<Type>>
		: std::type_identity<class local_for_class_of<Type>> {};

template <>
struct local_specialization<external_tag>
		: std::type_identity<class local_for_external> {};

} // namespace js::napi

// Specialize for `js::forward`. Makes `js::forward{local_of<Tag>}` infer
// `js::forward<Tag, ...>`.
namespace js {
template <class Tag>
struct forward_tag_for<napi::local_of<Tag>> : std::type_identity<Tag> {};
} // namespace js
