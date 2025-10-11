module;
#include <cstdint>
#include <string>
#include <variant>
export module isolated_js:value;
export import :bigint;
export import :date;
export import :dictionary.vector_of;
import :recursive_value;
import :tag;

namespace js {

// Any utf16 (canonical) or latin1 (optimized) string
export using string_t = std::variant<std::u16string, std::string>;

// Any (cloneable) object key
export using key_t = std::variant<int32_t, std::u16string, std::string>;

// Transferred runtime JS value
template <class Value>
using variant_value = std::variant<
	// `undefined`
	std::monostate,
	// `null`
	std::nullptr_t,
	// boolean
	bool,
	// number
	double,
	int32_t,
	// bigint
	bigint,
	uint64_t,
	// string
	std::u16string,
	std::string,
	// date
	js_clock::time_point,
	// object(s)
	dictionary<list_tag, key_t, Value>,
	dictionary<dictionary_tag, key_t, Value>
	// ---
	>;

export using value_t = recursive_value<variant_value>;

} // namespace js
