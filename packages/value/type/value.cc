module;
#include <boost/variant.hpp>
#include <cstdint>
#include <string>
#include <variant>
export module ivm.value:value;
import :date;
import :dictionary;
import :tag;

namespace ivm::value {

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
	int32_t,
	uint32_t,
	double,
	// bigint
	int64_t,
	uint64_t,
	// string
	std::string,
	std::u16string,
	// date
	js_clock::time_point,
	// object(s)
	value::dictionary<list_tag, key_t, boost::recursive_variant_>,
	value::dictionary<dictionary_tag, key_t, boost::recursive_variant_>>::type;

} // namespace ivm::value
