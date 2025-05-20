module;
#include <concepts>
#include <type_traits>
export module isolated_js.tag;

// Tag heirarchy helpers
template <class Tag>
struct tag_of : Tag {
		using con_tag = void;
		using tag_type = Tag;
};

template <class Tag>
struct con_tag_of : Tag {
		// `con_tag_for_t` will yield the tag that instantiated this
		using con_tag = Tag;
		using tag_type = Tag;
};

export namespace js {

// Should be defined unless `accept` and/or `visit` are specialized depending on whether this type
// sends or receives or both. For example: `tag_for<bool> -> boolean_tag`.
template <class Type>
struct tag_for;

struct value_tag {
		using con_tag = void;
		using tag_type = void;
};
struct external_tag : tag_of<value_tag> {};
struct primitive_tag : tag_of<value_tag> {};
struct function_tag : tag_of<primitive_tag> {};

// null & undefined
struct null_tag : tag_of<primitive_tag> {};
struct undefined_tag : tag_of<primitive_tag> {};
// Represents a value which is not present in an object, as opposed to one that is defined but
// `undefined`.
struct undefined_in_tag : tag_of<undefined_tag> {};

// numerics
struct boolean_tag : tag_of<primitive_tag> {};

struct number_tag : tag_of<primitive_tag> {};
template <class Type> struct number_tag_of : con_tag_of<number_tag> {};

struct bigint_tag : tag_of<value_tag> {};
template <class Type> struct bigint_tag_of : con_tag_of<bigint_tag> {};

// string & symbol
struct name_tag : tag_of<primitive_tag> {};
struct symbol_tag : tag_of<name_tag> {};
struct string_tag : tag_of<name_tag> {};
// string_tag = we're not sure, utf16 to be safe
// string_tag_of<char> = utf8
// string_tag_of<char16_t> = utf16
// string_tag_of<std::byte> = latin1
template <class Type> struct string_tag_of : con_tag_of<string_tag> {};

// objects
struct object_tag : tag_of<value_tag> {};

// Continuous packed array-like with integer keys and known (at runtime) length. Generally
// `arguments` or "trusted" arrays.
struct vector_tag : tag_of<object_tag> {};

// A vector of (statically) known size
template <std::size_t Size> struct vector_n_tag : tag_of<vector_tag> {};

// Tuple of elements, each possibly a different type.
template <std::size_t Element> struct tuple_tag : tag_of<object_tag> {};

// An object whose property keys are known at compile time.
template <std::size_t Properties> struct struct_tag : tag_of<object_tag> {};

// `{}`
struct dictionary_tag : tag_of<object_tag> {};

// `[]`
struct list_tag : tag_of<object_tag> {};

struct date_tag : tag_of<object_tag> {};
struct promise_tag : tag_of<object_tag> {};

// typed arrays & data view
struct array_buffer_tag : tag_of<object_tag> {};
struct shared_array_buffer_tag : tag_of<object_tag> {};

struct data_view_tag : tag_of<object_tag> {};
struct typed_array_tag : tag_of<object_tag> {};

struct float32_array_tag : tag_of<typed_array_tag> {};
struct float64_array_tag : tag_of<typed_array_tag> {};
struct int16_array_tag : tag_of<typed_array_tag> {};
struct int32_array_tag : tag_of<typed_array_tag> {};
struct int8_array_tag : tag_of<typed_array_tag> {};
struct uint16_array_tag : tag_of<typed_array_tag> {};
struct uint32_array_tag : tag_of<typed_array_tag> {};
struct uint8_array_tag : tag_of<typed_array_tag> {};
struct uint8_clamped_array_tag : tag_of<typed_array_tag> {};

// `auto` tag which accepts any tag inherited from `value_tag` while maintaining the true type of
// the tag.
template <class Type, class Tag = value_tag>
concept auto_tag = std::convertible_to<Type, Tag>;

// Get the covariant (most specific) tag for a given type
template <class Type>
using tag_for_t = tag_for<Type>::type;

// Get the contravariant (least specific) tag for a given type
template <class Type>
struct con_tag_for : std::type_identity<typename Type::con_tag> {};

template <class Type>
	requires std::same_as<typename Type::con_tag, void>
struct con_tag_for<Type> : std::type_identity<Type> {};

template <class Type>
using con_tag_for_t = con_tag_for<tag_for_t<Type>>::type;

} // namespace js
