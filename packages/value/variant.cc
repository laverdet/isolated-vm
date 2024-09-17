module;
#include <boost/variant.hpp>
#include <string>
#include <variant>
export module ivm.value:variant;
import :date;
import :dictionary;
import :primitive;
import :tag;

export namespace ivm::value {

using value_t = boost::make_recursive_variant<
	// `undefined`
	std::monostate,
	// `null`
	std::nullptr_t,
	// bool
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
	value::dictionary_value<list_tag, key_t, boost::recursive_variant_>,
	value::dictionary_value<dictionary_tag, key_t, boost::recursive_variant_>>::type;

} // namespace ivm::value
