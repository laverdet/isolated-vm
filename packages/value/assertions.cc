#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
import ivm.value;
using namespace std::literals;
#if __clang_major__ >= 20

namespace ivm::value {

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
enum class enum_test {
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

template <class Meta>
struct object_map<Meta, object_literal_one> : object_properties<Meta, object_literal_one> {
		template <auto Member>
		using property = object_properties<Meta, object_literal_one>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "integer", property<&object_literal_one::integer>::accept},
		};
};

template <class Meta>
struct object_map<Meta, object_literal> : object_properties<Meta, object_literal> {
		template <auto Member>
		using property = object_properties<Meta, object_literal>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "integer", property<&object_literal::integer>::accept},
			std::tuple{false, "number", property<&object_literal::number>::accept},
			std::tuple{true, "string", property<&object_literal::string>::accept},
		};
};

// Non-variant strict version
constexpr auto object_numeric = transfer_strict<object_literal_one>(std::array{
	std::pair{"integer"s, 1}
});
static_assert(object_numeric.integer == 1);

// Throwable values
using object_test_variant = std::variant<int, double, std::string>;
constexpr auto object_values_test = transfer<object_literal>(std::array{
	std::pair{"integer"s, object_test_variant{1}},
	std::pair{"number"s, object_test_variant{2.0}},
	std::pair{"string"s, object_test_variant{"hello"}},
});
static_assert(object_values_test.integer == 1);
static_assert(object_values_test.number == 2.0);
static_assert(object_values_test.string == "hello");

// Ensure custom acceptors work
struct specialized {
		constexpr auto operator==(const specialized& /*right*/) const -> bool { return true; };
};

template <class Meta>
struct accept<Meta, specialized> {
		constexpr auto operator()(object_tag /*tag*/, auto value) const -> specialized { return value; }
};

template <>
struct visit<specialized> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(specialized value, const auto& accept) const -> decltype(auto) {
			return accept(object_tag{}, value);
		}
};

static_assert(transfer<specialized>(specialized{}) == specialized{});
static_assert(variant_is_equal_to(transfer<std::variant<specialized>>(specialized{}), specialized{}));

// Discriminated unions
struct discriminated_one {
		std::string one;
		constexpr auto operator==(const discriminated_one& /*right*/) const -> bool { return true; };
};

struct discriminated_two {
		std::string two;
		constexpr auto operator==(const discriminated_two& /*right*/) const -> bool { return true; };
};

template <class Meta>
struct object_map<Meta, discriminated_one> : object_properties<Meta, discriminated_one> {
		template <auto Member>
		using property = object_properties<Meta, discriminated_one>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "one", property<&discriminated_one::one>::accept},
		};
};

template <class Meta>
struct object_map<Meta, discriminated_two> : object_properties<Meta, discriminated_two> {
		template <auto Member>
		using property = object_properties<Meta, discriminated_two>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "two", property<&discriminated_two::two>::accept},
		};
};

using discriminated_object = std::variant<discriminated_one, discriminated_two>;

template <class Meta>
struct discriminated_union<Meta, discriminated_object> : discriminated_alternatives<Meta, discriminated_object> {
		template <class Type>
		constexpr static auto alternative = &discriminated_alternatives<Meta, discriminated_object>::template alternative<Type>;

		constexpr static auto discriminant = "type";
		constexpr static auto alternatives = std::array{
			std::pair{"one", alternative<discriminated_one>},
			std::pair{"two", alternative<discriminated_two>},
		};
};

constexpr auto discriminated_with_one = std::array{
	std::pair{"type"s, "one"s},
	std::pair{"one"s, "left"s},
};

constexpr auto discriminated_with_two = std::array{
	std::pair{"type"s, "two"s},
	std::pair{"two"s, "right"s},
};

static_assert(variant_is_equal_to(transfer<discriminated_object>(discriminated_with_one), discriminated_one{.one = "left"s}));
static_assert(variant_is_equal_to(transfer<discriminated_object>(discriminated_with_two), discriminated_two{.two = "right"s}));

} // namespace ivm::value

#endif
