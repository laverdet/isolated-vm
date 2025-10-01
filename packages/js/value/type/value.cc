module;
#include <boost/variant.hpp>
#include <cstdint>
#include <string>
#include <variant>
export module isolated_js:value;
export import :bigint;
export import :date;
export import :dictionary.vector_of;
import :tag;

namespace js {

// Any (cloneable) object key
export using key_t = std::variant<int32_t, std::u16string, std::string>;

// Any latin1 or utf16 string
export using string_t = std::variant<std::u16string, std::string>;

export using value_t = boost::make_recursive_variant<
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
	dictionary<list_tag, key_t, boost::recursive_variant_>,
	dictionary<dictionary_tag, key_t, boost::recursive_variant_>>::type;

} // namespace js
