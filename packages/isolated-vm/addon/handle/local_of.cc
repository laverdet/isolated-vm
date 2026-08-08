export module isolated_vm:handle.local_of;
export import :handle.types;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

// Map of `local_of<Tag>` to its concrete implementation class
template <class Tag>
struct local_specialization;

// Tagged isolated_vm value. Each `local_of<T>` inherits from the tag before it.
export template <class Tag>
class local_of : public local_of<typename Tag::tag_type> {
	private:
		using local_type = local_specialization<Tag>::type;

	public:
		local_of() = default;
		using tag_type = Tag;

		// Constructor from tagged `runtime_handle` flavors
		explicit local_of(local_type value)
			requires(type<local_type> != type<runtime_handle>) :
				local_of{runtime_handle{value}} {}

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> local_of<To> { return local_of<To>::from(*this); }

		// Construct from any `value_handle`. Potentially unsafe.
		static auto from(runtime_handle value_) -> local_of<Tag> { return std::bit_cast<local_of<Tag>>(value_); }

		// Forward `make` to specialization
		static auto make(auto&&... args) -> local_of<Tag> { return local_type::make(std::forward<decltype(args)>(args)...); }

		// Dereference operators
		auto operator*() const -> local_type { return std::bit_cast<local_type>(runtime_handle{*this}); }
		auto operator->() const -> auto { return util::pointer_delegate{**this}; }
};

// Sentinel instantiation
template <>
class local_of<void> : public runtime_handle {
	public:
		using runtime_handle::runtime_handle;
		explicit local_of(runtime_handle value) : runtime_handle{value} {}
		local_of() = default;
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
template <class Tag>
struct local_specialization
		: local_specialization<typename Tag::tag_type> {};

template <>
struct local_specialization<void>
		: std::type_identity<runtime_handle> {};

template <>
struct local_specialization<value_tag>
		: std::type_identity<class local_for_value> {};

template <>
struct local_specialization<primitive_tag>
		: std::type_identity<class local_for_primitive> {};

template <>
struct local_specialization<undefined_tag>
		: std::type_identity<class local_for_undefined> {};

template <>
struct local_specialization<null_tag>
		: std::type_identity<class local_for_null> {};

template <>
struct local_specialization<boolean_tag>
		: std::type_identity<class local_for_boolean> {};

template <>
struct local_specialization<number_tag>
		: std::type_identity<class local_for_number> {};

template <>
struct local_specialization<name_tag>
		: std::type_identity<class local_for_name> {};

template <>
struct local_specialization<string_tag>
		: std::type_identity<class local_for_string> {};

template <>
struct local_specialization<bigint_tag>
		: std::type_identity<class local_for_bigint> {};

template <>
struct local_specialization<function_tag>
		: std::type_identity<class local_for_function> {};

template <>
struct local_specialization<object_tag>
		: std::type_identity<class local_for_object> {};

template <>
struct local_specialization<record_tag>
		: std::type_identity<class local_for_record> {};

template <>
struct local_specialization<list_tag>
		: std::type_identity<class local_for_array> {};

template <>
struct local_specialization<data_block_tag>
		: std::type_identity<class local_for_data_block> {};

template <>
struct local_specialization<array_buffer_tag>
		: std::type_identity<class local_for_array_buffer> {};

template <>
struct local_specialization<array_buffer_view_tag>
		: std::type_identity<class local_for_array_buffer_view> {};

template <>
struct local_specialization<prototype_tag>
		: std::type_identity<class local_for_prototype> {};

} // namespace isolated_vm

// Specialize for `js::forward`. Makes `js::forward{local_of<Tag>}` infer
// `js::forward<Tag, ...>`.
namespace js {
template <class Tag>
struct forward_tag_for<isolated_vm::local_of<Tag>> : std::type_identity<Tag> {};
} // namespace js
