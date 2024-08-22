module;
#include <boost/variant.hpp>
#include <string>
#include <variant>
export module ivm.value:variant;
import :date;
import :dictionary;
import :primitive;

export namespace ivm::value {

using value_t = boost::make_recursive_variant<
	std::monostate, // `undefined`
	std::nullptr_t, // `null`
	bool,
	int32_t,
	uint32_t,
	double,
	std::string,
	std::u16string,
	js_clock::time_point,
	value::dictionary_value<list_tag, key_t, boost::recursive_variant_>,
	value::dictionary_value<dictionary_tag, key_t, boost::recursive_variant_>>::type;

} // namespace ivm::value
