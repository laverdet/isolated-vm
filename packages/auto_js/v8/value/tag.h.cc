export module v8_js:value.tag;
import auto_js;
import std;
import util;
import v8;

namespace js {

constexpr auto tag_to_v8_fn = util::overloaded{
	[](value_tag /*tag*/) -> std::type_identity<v8::Value> { return {}; },

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

template <class Tag>
using tag_to_v8 = type_t<tag_to_v8_fn(Tag{})>;

template <class Type>
using v8_to_tag = decltype(v8_to_tag_fn(type<Type>));

} // namespace js
