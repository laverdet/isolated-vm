export module napi_js:handle.types;
import auto_js;
import nodejs;

namespace js::napi {

// `value_handle` is the base class of `value<T>`, and `bound_value<T>`.
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

// `value` forward declarations
export template <class Tag = value_tag>
class value;

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

// Map of `value<Tag>` / `bound_value<Tag>` to concrete implementation classes.
template <class Tag>
struct value_specialization {
		using value_type = value_next<Tag>;
		using bound_type = bound_value_next<Tag>;
};

// Typed specializations
template <>
struct value_specialization<undefined_tag> {
		using value_type = class value_for_undefined;
		using bound_type = bound_value_next<undefined_tag>;
};

template <>
struct value_specialization<null_tag> {
		using value_type = class value_for_null;
		using bound_type = bound_value_next<null_tag>;
};

template <>
struct value_specialization<boolean_tag> {
		using value_type = class value_for_boolean;
		using bound_type = class bound_value_for_boolean;
};

template <>
struct value_specialization<number_tag> {
		using value_type = class value_for_number;
		using bound_type = class bound_value_for_number;
};

template <>
struct value_specialization<bigint_tag> {
		using value_type = class value_for_bigint;
		using bound_type = class bound_value_for_bigint;
};

template <>
struct value_specialization<string_tag> {
		using value_type = class value_for_string;
		using bound_type = class bound_value_for_string;
};

template <>
struct value_specialization<date_tag> {
		using value_type = class value_for_date;
		using bound_type = class bound_value_for_date;
};

template <>
struct value_specialization<object_tag> {
		using value_type = class value_for_object;
		using bound_type = class bound_value_for_object;
};

template <>
struct value_specialization<function_tag> {
		using value_type = class value_for_function;
		using bound_type = bound_value_next<function_tag>;
};

template <>
struct value_specialization<array_buffer_tag> {
		using value_type = class value_for_array_buffer;
		using bound_type = class bound_value_for_array_buffer;
};

template <>
struct value_specialization<shared_array_buffer_tag> {
		using value_type = class value_for_shared_array_buffer;
		using bound_type = class bound_value_for_shared_array_buffer;
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
struct value_specialization<class_tag_of<Type>> {
		using value_type = value_for_class_of<Type>;
		using bound_type = bound_value_next<class_tag_of<Type>>;
};

template <>
struct value_specialization<external_tag> {
		using value_type = class value_for_external;
		using bound_type = class bound_value_for_external;
};

template <>
struct value_specialization<vector_tag> {
		using value_type = value_next<vector_tag>;
		using bound_type = class bound_value_for_vector;
};

template <>
struct value_specialization<list_tag> {
		using value_type = value_next<list_tag>;
		using bound_type = class bound_value_for_list;
};

template <>
struct value_specialization<dictionary_tag> {
		using value_type = value_next<dictionary_tag>;
		using bound_type = class bound_value_for_dictionary;
};

} // namespace js::napi
