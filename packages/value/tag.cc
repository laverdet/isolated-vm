module;
#include <concepts>
export module ivm.value:tag;

export namespace ivm::value {

struct value_tag {};
struct primitive_tag : value_tag {};
struct external_tag : value_tag {};

struct null_tag : primitive_tag {};
struct undefined_tag : primitive_tag {};

struct boolean_tag : primitive_tag {};

struct key_tag : primitive_tag {};
struct numeric_tag : key_tag {};
struct string_tag : key_tag {};

struct object_tag : value_tag {};
struct dictionary_tag : object_tag {};
struct list_tag : object_tag {};
struct date_tag : object_tag {};
struct promise_tag : object_tag {};
// Continuous array-like with integer keys and known length. Generally `arguments` or "trusted"
// arrays.
struct vector_tag : object_tag {};

struct array_buffer_tag : object_tag {};
struct shared_array_buffer_tag : object_tag {};

struct data_view_tag : object_tag {};
struct typed_array_tag : object_tag {};

struct float32_array_tag : typed_array_tag {};
struct float64_array_tag : typed_array_tag {};
struct int16_array_tag : typed_array_tag {};
struct int32_array_tag : typed_array_tag {};
struct int8_array_tag : typed_array_tag {};
struct uint16_array_tag : typed_array_tag {};
struct uint32_array_tag : typed_array_tag {};
struct uint8_array_tag : typed_array_tag {};
struct uint8_clamped_array_tag : typed_array_tag {};

// `auto` tag which accepts any tag inherited from `value_tag` while maintaining the true type of
// the tag.
template <class Type>
concept auto_tag = std::convertible_to<Type, value_tag>;

// Should be defined unless `accept` and/or `visit` are specialized depending on whether this type
// sends or receives or both.
template <class Type>
struct tag_for {
		using type = value_tag;
};

template <class Type>
using tag_for_t = tag_for<Type>::type;

} // namespace ivm::value
