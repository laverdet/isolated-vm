module;
#include <boost/variant.hpp>
#include <cstdint>
#include <string>
#include <variant>
export module isolated_js.value;
import isolated_js.date;
import isolated_js.dictionary.vector_of;
import isolated_js.tag;

namespace js {

// Any (cloneable) object key
export using key_t = std::variant<int32_t, std::string, std::u16string>;

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
	uint32_t,
	// bigint
	int64_t,
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
