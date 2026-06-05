export module util:type_traits;
export import :type_traits.function_traits;
export import :type_traits.transform;
export import :type_traits.type_of;
export import :type_traits.type_pack;
import std;

namespace util {

// Check if casting from one numeric to another can be done without loss. It is probably missing a
// plethora of exotic cases but I think this covers the fundamentals.
export template <class From, class To>
struct safe_numeric_conversion {
		using limits_from = std::numeric_limits<From>;
		using limits_to = std::numeric_limits<To>;
		constexpr static auto value =
			// Check signed / floating point compatibility
			(limits_to::is_signed || !limits_from::is_signed) &&
			(limits_from::is_integer || !limits_to::is_integer) &&
			// Check precision
			(limits_to::digits >= limits_from::digits) &&
			// Check range
			(limits_from::lowest() >= limits_to::lowest()) &&
			(limits_from::max() <= limits_to::max());
};

export template <class From, class To>
constexpr auto safe_numeric_conversion_v = safe_numeric_conversion<From, To>::value;

static_assert(safe_numeric_conversion<std::int32_t, std::int64_t>::value);
static_assert(!safe_numeric_conversion<std::int32_t, std::uint64_t>::value);
static_assert(!safe_numeric_conversion<std::int64_t, double>::value);
static_assert(safe_numeric_conversion<std::int32_t, double>::value);
static_assert(safe_numeric_conversion<float, double>::value);
static_assert(!safe_numeric_conversion<double, float>::value);

// Resolves to the type of every type in the pack. If any type differs then it is ill-formed.
template <class... Type>
struct identical_type;

template <class... Type>
using identical_type_t = identical_type<Type...>::type;

template <class Type>
struct identical_type<Type> : std::type_identity<Type> {};

template <class Type, class... Rest>
struct identical_type<Type, Type, Rest...> : identical_type<Type, Rest...> {};

} // namespace util
