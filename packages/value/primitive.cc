module;
#include <cstdint>
#include <string>
#include <variant>
export module ivm.value:primitive;
import :tag;

namespace ivm::value {

// `undefined`
template <>
struct tag_for<std::monostate> {
		using type = undefined_tag;
};

// `null`
template <>
struct tag_for<std::nullptr_t> {
		using type = null_tag;
};

// `boolean`
template <>
struct tag_for<bool> {
		using type = boolean_tag;
};

// `number`
export using number_t = std::variant<int32_t, uint32_t, double>;

template <>
struct tag_for<int32_t> {
		using type = number_tag_of<int32_t>;
};

template <>
struct tag_for<uint32_t> {
		using type = number_tag_of<uint32_t>;
};

template <>
struct tag_for<double> {
		using type = number_tag_of<double>;
};

// `bigint`
export using bigint_t = std::variant<int64_t, uint64_t>;

template <>
struct tag_for<int64_t> {
		using type = number_tag_of<int64_t>;
};

template <>
struct tag_for<uint64_t> {
		using type = number_tag_of<uint64_t>;
};

// `string`
export using string_t = std::variant<std::string, std::u16string>;

template <>
struct tag_for<std::string> {
		using type = string_tag_of<std::string>;
};

template <>
struct tag_for<std::string_view> {
		using type = string_tag_of<std::string_view>;
};

template <>
struct tag_for<std::u16string> {
		using type = string_tag_of<std::u16string>;
};

// Any (cloneable) object key
export using key_t = std::variant<int32_t, std::string, std::u16string>;

} // namespace ivm::value
