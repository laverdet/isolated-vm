export module auto_js:value;
export import :intrinsics.array_buffer;
export import :intrinsics.bigint;
export import :intrinsics.date;
export import :vector.vector_of;
import :variant.types;
import std;

namespace js {

// Result of `typeof` operator
// nb: If you name it `typeof`, which is a totally valid C++ identifier, then clang-tidy can't
// handle it and puts spaces everywhere.
export enum class typeof_kind : std::uint8_t {
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

// Dictionary key type for `js::value_t`
using value_key_type = std::variant<
	std::int32_t,
	reference_of<std::u16string>,
	reference_of<std::string>>;

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
	reference_of<double>,
	std::int32_t,
	// bigint
	reference_of<bigint>,
	reference_of<std::int64_t>,
	// strings
	reference_of<std::u16string>,
	reference_of<std::string>,
	// date
	reference_of<js_clock::time_point>,
	// object(s)
	reference_of<dictionary<list_tag, value_key_type, Value>>,
	reference_of<dictionary<dictionary_tag, value_key_type, Value>>,
	// data blocks
	reference_of<array_buffer>,
	reference_of<shared_array_buffer>,
	// typed arrays & data view
	reference_of<typed_array<double>>,
	reference_of<typed_array<float>>,
	reference_of<typed_array<std::int16_t>>,
	reference_of<typed_array<std::int32_t>>,
	reference_of<typed_array<std::int64_t>>,
	reference_of<typed_array<std::int8_t>>,
	reference_of<typed_array<std::byte>>,
	reference_of<typed_array<std::uint16_t>>,
	reference_of<typed_array<std::uint32_t>>,
	reference_of<typed_array<std::uint64_t>>,
	reference_of<typed_array<std::uint8_t>>
	// reference_of<typed_array<void>>
	// ---
	>;

using value_holder_t = referential_variant<variant_value>;
export using value_t = referential_value<value_holder_t, value_holder_t>;
export using values_vector_t = referential_value<std::vector<value_holder_t>, value_holder_t>;

} // namespace js
