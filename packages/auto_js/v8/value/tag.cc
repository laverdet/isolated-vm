export module v8_js:value.tag;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

// These helper classes are provided to avoid extra visit inspections on tagged values
export class BigInt64 : public v8::BigInt {};
export class BigIntU64 : public v8::BigInt {};
export class BigIntWords : public v8::BigInt {};
export class Double : public v8::Number {};
export class Null : public v8::Primitive {};
export class StringOneByte : public v8::String {};
export class StringTwoByte : public v8::String {};
export class Undefined : public v8::Primitive {};

constexpr auto tag_to_v8_fn = util::overloaded{
	[](value_tag /*tag*/) -> std::type_identity<v8::Value> { return {}; },

	// primitives
	[](bigint_tag /*tag*/) -> std::type_identity<v8::BigInt> { return {}; },
	[](bigint_tag_of<bigint> /*tag*/) -> std::type_identity<iv8::BigIntWords> { return {}; },
	[](bigint_tag_of<std::int64_t> /*tag*/) -> std::type_identity<iv8::BigInt64> { return {}; },
	[](bigint_tag_of<std::uint64_t> /*tag*/) -> std::type_identity<iv8::BigIntU64> { return {}; },
	[](boolean_tag /*tag*/) -> std::type_identity<v8::Boolean> { return {}; },
	[](name_tag /*tag*/) -> std::type_identity<v8::Name> { return {}; },
	[](number_tag /*tag*/) -> std::type_identity<v8::Number> { return {}; },
	[](number_tag_of<double> /*tag*/) -> std::type_identity<iv8::Double> { return {}; },
	[](number_tag_of<std::int32_t> /*tag*/) -> std::type_identity<v8::Int32> { return {}; },
	[](number_tag_of<std::uint32_t> /*tag*/) -> std::type_identity<v8::Uint32> { return {}; },
	[](primitive_tag /*tag*/) -> std::type_identity<v8::Primitive> { return {}; },
	[](string_tag /*tag*/) -> std::type_identity<v8::String> { return {}; },
	[](string_tag_of<char> /*tag*/) -> std::type_identity<iv8::StringOneByte> { return {}; },
	[](string_tag_of<char16_t> /*tag*/) -> std::type_identity<iv8::StringTwoByte> { return {}; },
	[](symbol_tag /*tag*/) -> std::type_identity<v8::Symbol> { return {}; },

	// objects
	[](date_tag /*tag*/) -> std::type_identity<v8::Date> { return {}; },
	[](function_tag /*tag*/) -> std::type_identity<v8::Function> { return {}; },
	[](list_tag /*tag*/) -> std::type_identity<v8::Array> { return {}; },
	[](object_tag /*tag*/) -> std::type_identity<v8::Object> { return {}; },
	[](promise_tag /*tag*/) -> std::type_identity<v8::Promise> { return {}; },

	// typed arrays
	[](typed_array_tag_of<double> /*tag*/) -> std::type_identity<v8::Float64Array> { return {}; },
	[](typed_array_tag_of<float> /*tag*/) -> std::type_identity<v8::Float32Array> { return {}; },
	[](typed_array_tag_of<std::byte> /*tag*/) -> std::type_identity<v8::Uint8ClampedArray> { return {}; },
	[](typed_array_tag_of<std::int16_t> /*tag*/) -> std::type_identity<v8::Int16Array> { return {}; },
	[](typed_array_tag_of<std::int32_t> /*tag*/) -> std::type_identity<v8::Int32Array> { return {}; },
	[](typed_array_tag_of<std::int64_t> /*tag*/) -> std::type_identity<v8::BigInt64Array> { return {}; },
	[](typed_array_tag_of<std::int8_t> /*tag*/) -> std::type_identity<v8::Int8Array> { return {}; },
	[](typed_array_tag_of<std::uint16_t> /*tag*/) -> std::type_identity<v8::Uint16Array> { return {}; },
	[](typed_array_tag_of<std::uint32_t> /*tag*/) -> std::type_identity<v8::Uint32Array> { return {}; },
	[](typed_array_tag_of<std::uint64_t> /*tag*/) -> std::type_identity<v8::BigUint64Array> { return {}; },
	[](typed_array_tag_of<std::uint8_t> /*tag*/) -> std::type_identity<v8::Uint8Array> { return {}; },
};

constexpr auto v8_to_tag_fn = util::overloaded{
	[](std::type_identity<v8::Value> /*type*/) -> value_tag { return {}; },

	// primitives
	[](std::type_identity<iv8::BigInt64> /*type*/) -> bigint_tag_of<std::int64_t> { return {}; },
	[](std::type_identity<iv8::BigIntU64> /*type*/) -> bigint_tag_of<std::uint64_t> { return {}; },
	[](std::type_identity<iv8::BigIntWords> /*type*/) -> bigint_tag_of<bigint> { return {}; },
	[](std::type_identity<iv8::Double> /*type*/) -> number_tag_of<double> { return {}; },
	[](std::type_identity<iv8::StringOneByte> /*type*/) -> string_tag_of<char> { return {}; },
	[](std::type_identity<iv8::StringTwoByte> /*type*/) -> string_tag_of<char16_t> { return {}; },
	[](std::type_identity<v8::BigInt> /*type*/) -> bigint_tag { return {}; },
	[](std::type_identity<v8::Boolean> /*type*/) -> boolean_tag { return {}; },
	[](std::type_identity<v8::Int32> /*type*/) -> number_tag_of<std::int32_t> { return {}; },
	[](std::type_identity<v8::Name> /*type*/) -> name_tag { return {}; },
	[](std::type_identity<v8::Number> /*type*/) -> number_tag { return {}; },
	[](std::type_identity<v8::Primitive> /*type*/) -> primitive_tag { return {}; },
	[](std::type_identity<v8::String> /*type*/) -> string_tag { return {}; },
	[](std::type_identity<v8::Symbol> /*type*/) -> symbol_tag { return {}; },

	// objects
	[](std::type_identity<v8::Array> /*type*/) -> list_tag { return {}; },
	[](std::type_identity<v8::Date> /*type*/) -> date_tag { return {}; },
	[](std::type_identity<v8::Function> /*type*/) -> function_tag { return {}; },
	[](std::type_identity<v8::Object> /*type*/) -> object_tag { return {}; },
	[](std::type_identity<v8::Promise> /*type*/) -> promise_tag { return {}; },

	// typed arrays
	[](std::type_identity<v8::BigInt64Array> /*type*/) -> typed_array_tag_of<std::int64_t> { return {}; },
	[](std::type_identity<v8::BigUint64Array> /*type*/) -> typed_array_tag_of<std::uint64_t> { return {}; },
	[](std::type_identity<v8::Float32Array> /*type*/) -> typed_array_tag_of<float> { return {}; },
	[](std::type_identity<v8::Float64Array> /*type*/) -> typed_array_tag_of<double> { return {}; },
	[](std::type_identity<v8::Int16Array> /*type*/) -> typed_array_tag_of<std::int16_t> { return {}; },
	[](std::type_identity<v8::Int32Array> /*type*/) -> typed_array_tag_of<std::int32_t> { return {}; },
	[](std::type_identity<v8::Int8Array> /*type*/) -> typed_array_tag_of<std::int8_t> { return {}; },
	[](std::type_identity<v8::Uint16Array> /*type*/) -> typed_array_tag_of<std::uint16_t> { return {}; },
	[](std::type_identity<v8::Uint32Array> /*type*/) -> typed_array_tag_of<std::uint32_t> { return {}; },
	[](std::type_identity<v8::Uint8Array> /*type*/) -> typed_array_tag_of<std::uint8_t> { return {}; },
	[](std::type_identity<v8::Uint8ClampedArray> /*type*/) -> typed_array_tag_of<std::byte> { return {}; },
};

export template <class Tag>
using tag_to_v8 = type_t<tag_to_v8_fn(Tag{})>;

export template <class Type>
using v8_to_tag = decltype(v8_to_tag_fn(type<Type>));

} // namespace js::iv8
