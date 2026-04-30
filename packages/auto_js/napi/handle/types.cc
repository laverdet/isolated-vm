export module napi_js:handle.types;
import auto_js;
import nodejs;

namespace js::napi {

// `value_handle` is the base class of `value_of<T>`, and `bound_value<T>`.
class value_handle {
	protected:
		explicit value_handle(napi_value value) :
				value_{value} {}

	public:
		value_handle() = default;

		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const { return value_; }

		// Check empty value
		explicit operator bool() const { return value_ != nullptr; }

	private:
		napi_value value_{};
};

// `value_of` forward declarations
export template <class Tag = value_tag>
class value_of;

template <class Tag>
class value_next;

template <class Type>
class value_for_class_of;

template <class Type>
class value_for_typed_array_of;

// `bound_value` forward declarations
export template <class Tag>
class bound_value;

template <class Tag>
class bound_value_next;

template <class Type>
class bound_value_for_typed_array_of;

// Map of `value_of<Tag>` / `bound_value<Tag>` to concrete implementation classes.
template <class Tag>
struct value_defaults {
		using value_type = value_next<Tag>;
		using bound_type = bound_value_next<Tag>;
};

template <class Tag>
struct value_specialization : value_defaults<Tag> {};

// Typed specializations
template <>
struct value_specialization<boolean_tag> : value_defaults<boolean_tag> {
		using bound_type = class bound_value_for_boolean;
};

template <>
struct value_specialization<number_tag> : value_defaults<number_tag> {
		using bound_type = class bound_value_for_number;
};

template <>
struct value_specialization<bigint_tag> : value_defaults<bigint_tag> {
		using bound_type = class bound_value_for_bigint;
};

template <>
struct value_specialization<string_tag> {
		using value_type = class value_for_string;
		using bound_type = class bound_value_for_string;
};

template <>
struct value_specialization<date_tag> : value_defaults<date_tag> {
		using bound_type = class bound_value_for_date;
};

template <>
struct value_specialization<object_tag> {
		using value_type = class value_for_object;
		using bound_type = class bound_value_for_object;
};

template <>
struct value_specialization<record_tag> : value_defaults<record_tag> {
		using bound_type = class bound_value_for_record;
};

template <>
struct value_specialization<function_tag> : value_defaults<function_tag> {
		using value_type = class value_for_function;
};

template <>
struct value_specialization<array_buffer_tag> : value_defaults<array_buffer_tag> {
		using bound_type = class bound_value_for_array_buffer;
};

template <>
struct value_specialization<shared_array_buffer_tag> : value_defaults<shared_array_buffer_tag> {
		using bound_type = class bound_value_for_shared_array_buffer;
};

template <>
struct value_specialization<array_buffer_view_tag> : value_defaults<array_buffer_view_tag> {
		using bound_type = class bound_value_for_array_buffer_view;
};

template <>
struct value_specialization<typed_array_tag> {
		using value_type = class value_for_typed_array;
		using bound_type = class bound_value_for_typed_array;
};

template <class Type>
struct value_specialization<typed_array_tag_of<Type>> {
		using value_type = value_for_typed_array_of<Type>;
		using bound_type = bound_value_for_typed_array_of<Type>;
};

template <>
struct value_specialization<data_view_tag> {
		using value_type = class value_for_data_view;
		using bound_type = class bound_value_for_data_view;
};

template <class Type>
struct value_specialization<class_tag_of<Type>> : value_defaults<class_tag_of<Type>> {
		using value_type = value_for_class_of<Type>;
};

template <>
struct value_specialization<external_tag> {
		using value_type = class value_for_external;
		using bound_type = class bound_value_for_external;
};

template <>
struct value_specialization<vector_tag> : value_defaults<vector_tag> {
		using bound_type = class bound_value_for_vector;
};

} // namespace js::napi
