#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
import isolated_js;
using namespace std::literals;
#if __clang_major__ >= 19

namespace js {

// Narrowing sanity check. We want to narrow in non-variant cases because if the visitor has a
// string and the acceptor wants a string_view, that should be fine. Or if v8 gives us a uint32_t
// and we want an int32_t then that is probably fine.
static_assert(transfer_strict<int>(1.0) == 1);
static_assert(transfer_strict<double>(1) == 1.0);
static_assert(transfer_strict<std::string>("hello"sv) == "hello"s);

// Variants
template <class Variant, class Type>
constexpr auto variant_is_equal_to(const Variant& variant, const Type& value) {
	return std::holds_alternative<Type>(variant) && std::get<Type>(variant) == value;
}

static_assert(variant_is_equal_to(transfer_strict<std::variant<int, double>>(1.0), 1.0));
constexpr auto string_variant = std::variant<std::string>{"hello"};
constexpr auto visited_string = transfer<std::variant<std::monostate, std::string>>(string_variant);
static_assert(variant_is_equal_to(visited_string, "hello"s));

// Optional
constexpr auto optional_value = transfer<std::optional<int>>(std::monostate{});
static_assert(optional_value == std::nullopt);
constexpr auto just_optional_value = transfer<std::optional<int>>(1);
static_assert(just_optional_value == 1);
static_assert(transfer<std::optional<double>>(1) == 1.0);

// Enumerations
enum class enum_test : std::int8_t {
	first,
	second,
	third,
};

template <>
struct enum_values<enum_test> {
		constexpr static auto values = std::array{
			std::pair{"first", enum_test::first},
			std::pair{"second", enum_test::second},
			std::pair{"third", enum_test::third}
		};
};

static_assert(transfer<enum_test>("first"sv) == enum_test::first);
static_assert(transfer<enum_test>("second"sv) == enum_test::second);
static_assert(transfer<enum_test>("third"sv) == enum_test::third);

// Objects
struct object_literal_one {
		int integer{};
};
struct object_literal {
		int integer{};
		double number{};
		std::string string;
};

template <>
struct object_properties<object_literal_one> {
		constexpr static auto properties = std::tuple{
			member<"integer", &object_literal_one::integer, false>{},
		};
};

template <>
struct object_properties<object_literal> {
		constexpr static auto properties = std::tuple{
			member<"integer", &object_literal::integer, false>{},
			member<"number", &object_literal::number, false>{},
			member<"string", &object_literal::string>{},
		};
};

// Non-variant strict version
constexpr auto object_numeric = transfer_strict<object_literal_one>(dictionary{
	{std::pair{"integer"s, 1}}
});
static_assert(object_numeric.integer == 1);

// Throwable values
using object_test_variant = std::variant<int, double, std::string>;
constexpr auto object_values_test = transfer<object_literal>(dictionary{{
	std::pair{"integer"s, object_test_variant{1}},
	std::pair{"number"s, object_test_variant{2.0}},
	std::pair{"string"s, object_test_variant{"hello"}},
}});
static_assert(object_values_test.integer == 1);
static_assert(object_values_test.number == 2.0);
static_assert(object_values_test.string == "hello");
static_assert(
	transfer<dictionary<dictionary_tag, std::string, object_test_variant>>(object_values_test) ==
	dictionary{{
		std::pair{"integer"s, object_test_variant{1}},
		std::pair{"number"s, object_test_variant{2.0}},
		std::pair{"string"s, object_test_variant{"hello"}},
	}}
);

// Ensure custom acceptors work
struct specialized {
		constexpr auto operator==(const specialized& /*right*/) const -> bool { return true; };
};

template <>
struct accept<void, specialized> : accept<void, void> {
		using accept<void, void>::accept;
		constexpr auto operator()(object_tag /*tag*/, auto value) const -> specialized { return value; }
};

template <>
struct visit<void, specialized> : visit<void, void> {
		using visit<void, void>::visit;
		constexpr auto operator()(specialized value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(object_tag{}, value);
		}
};

static_assert(transfer<specialized>(specialized{}) == specialized{});
static_assert(variant_is_equal_to(transfer<std::variant<specialized>>(specialized{}), specialized{}));

// Discriminated unions
struct union_alternative_one {
		std::string one;
		constexpr auto operator==(const union_alternative_one& right) const -> bool { return one == right.one; };
};

struct union_alternative_two {
		std::string two;
		constexpr auto operator==(const union_alternative_two& right) const -> bool { return two == right.two; };
};

template <>
struct object_properties<union_alternative_one> {
		constexpr static auto properties = std::tuple{
			member<"one", &union_alternative_one::one, false>{},
		};
};

template <>
struct object_properties<union_alternative_two> {
		constexpr static auto properties = std::tuple{
			member<"two", &union_alternative_two::two, false>{},
		};
};

using union_object = std::variant<union_alternative_one, union_alternative_two>;

template <>
struct union_of<union_object> {
		constexpr static auto& discriminant = "type";
		constexpr static auto alternatives = std::tuple{
			alternative<union_alternative_one>{"one"},
			alternative<union_alternative_two>{"two"},
		};
};

constexpr auto discriminated_with_one = transfer<union_object>(dictionary{{
	std::pair{"type"s, "one"s},
	std::pair{"one"s, "left"s},
}});

constexpr auto discriminated_with_two = transfer<union_object>(dictionary{{
	std::pair{"type"s, "two"s},
	std::pair{"two"s, "right"s},
}});

static_assert(variant_is_equal_to(discriminated_with_one, union_alternative_one{.one = "left"s}));
static_assert(variant_is_equal_to(discriminated_with_two, union_alternative_two{.two = "right"s}));

} // namespace js

#endif
