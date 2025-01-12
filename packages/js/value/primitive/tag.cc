module;
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.js:primitive.tag;
import :date;
import :tag;
import ivm.utility;

namespace js {

// `undefined`
template <>
struct tag_for<std::monostate> : std::type_identity<undefined_tag> {};

// `null`
template <>
struct tag_for<std::nullptr_t> : std::type_identity<null_tag> {};

// `boolean`
template <>
struct tag_for<bool> : std::type_identity<boolean_tag> {};

// `number`
template <>
struct tag_for<int32_t> : std::type_identity<number_tag_of<int32_t>> {};

template <>
struct tag_for<uint32_t> : std::type_identity<number_tag_of<uint32_t>> {};

template <>
struct tag_for<double> : std::type_identity<number_tag_of<double>> {};

// `bigint`
template <>
struct tag_for<int64_t> : std::type_identity<bigint_tag_of<int64_t>> {};

template <>
struct tag_for<uint64_t> : std::type_identity<bigint_tag_of<uint64_t>> {};

// `date`
template <>
struct tag_for<js_clock::time_point> : std::type_identity<date_tag> {};

// `string`
template <>
struct tag_for<const char*> : std::type_identity<string_tag_of<char>> {};

template <>
struct tag_for<std::string> : std::type_identity<string_tag_of<char>> {};

template <>
struct tag_for<std::string_view> : std::type_identity<string_tag_of<char>> {};

template <>
struct tag_for<std::u16string> : std::type_identity<string_tag_of<char16_t>> {};

} // namespace js
