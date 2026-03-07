module;
#include <cstdint>
#include <string>
#include <variant>
export module auto_js:value;
export import :intrinsics.bigint;
export import :intrinsics.date;
export import :vector.vector_of;
import :variant.types;
import :tag;

namespace js {

// Result of `typeof` operator
// nb: If you name it `typeof`, which is a totally valid C++ identifier, then clang-tidy can't
// handle it and puts spaces everywhere.
export enum class typeof_kind : uint8_t {
	bigint,
	boolean,
	function,
	null,
	number,
	object,
	string,
	symbol,
	undefined,
};

// Any utf16 (canonical) or latin1 (optimized) string
export using string_t = std::variant<std::u16string, std::string>;

// Transferred runtime JS value
// TODO: `Value` is used at the dictionary `key` type here which means acceptors of these values
// will have a bunch of unnecessary exception logic for values like `bigint` which cannot be keys.
template <class Value>
using variant_value = std::variant<
	// `undefined`
	std::monostate,
	// `null`
	std::nullptr_t,
	// boolean
	bool,
	// number
	reference_of<double>,
	int32_t,
	// bigint
	reference_of<bigint>,
	reference_of<uint64_t>,
	// strings
	reference_of<std::u16string>,
	reference_of<std::string>,
	// date
	reference_of<js_clock::time_point>,
	// object(s)
	reference_of<dictionary<list_tag, Value, Value>>,
	reference_of<dictionary<dictionary_tag, Value, Value>>
	// ---
	>;

export using value_t = referential_variant<variant_value>;

} // namespace js
