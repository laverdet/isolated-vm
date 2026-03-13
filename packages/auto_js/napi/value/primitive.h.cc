module;
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
export module napi_js:primitive;
import :api;
import :bound_value;
import :environment_fwd;
import :value_handle;

namespace js::napi {

// undefined
class value_for_undefined : public value_next<undefined_tag> {
	public:
		using value_next<undefined_tag>::value_next;
		static auto make(const environment& env) -> value<undefined_tag>;
		static auto make(const environment& env, std::monostate /*undefined*/) -> value<undefined_tag> { return make(env); }
};

// null
class value_for_null : public value_next<null_tag> {
	public:
		using value_next<null_tag>::value_next;
		static auto make(const environment& env) -> value<null_tag>;
		static auto make(const environment& env, std::nullptr_t /*null*/) -> value<null_tag> { return make(env); }
};

// boolean
class value_for_boolean : public value_next<boolean_tag> {
	public:
		using value_next<boolean_tag>::value_next;
		static auto make(const environment& env, bool boolean) -> value<boolean_tag>;
};

class bound_value_for_boolean : public bound_value_next<boolean_tag> {
	public:
		using bound_value_next<boolean_tag>::bound_value_next;
		[[nodiscard]] explicit operator bool() const;
};

// number
class value_for_number : public value_next<number_tag> {
	public:
		using value_next<number_tag>::value_next;
		static auto make(const environment& env, double number) -> value<number_tag>;
		static auto make(const environment& env, int32_t number) -> value<number_tag>;
		static auto make(const environment& env, int64_t number) -> value<number_tag>;
		static auto make(const environment& env, uint32_t number) -> value<number_tag>;

		static auto make_property_name(const environment& env, int32_t number) -> value<number_tag> { return make(env, number); }
};

class bound_value_for_number : public bound_value_next<number_tag> {
	public:
		using bound_value_next<number_tag>::bound_value_next;
		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator int32_t() const;
		[[nodiscard]] explicit operator int64_t() const;
		[[nodiscard]] explicit operator uint32_t() const;
};

// bigint
class value_for_bigint : public value_next<bigint_tag> {
	public:
		using value_next<bigint_tag>::value_next;
		static auto make(const environment& env, const bigint& number) -> value<bigint_tag>;
		static auto make(const environment& env, int64_t number) -> value<bigint_tag>;
		static auto make(const environment& env, uint64_t number) -> value<bigint_tag>;
};

class bound_value_for_bigint : public bound_value_next<bigint_tag> {
	public:
		using bound_value_next<bigint_tag>::bound_value_next;
		[[nodiscard]] explicit operator bigint() const;
		[[nodiscard]] explicit operator int64_t() const;
		[[nodiscard]] explicit operator uint64_t() const;
};

// string
class value_for_string : public value_next<string_tag> {
	public:
		using value_next<string_tag>::value_next;
		static auto make(const environment& env, std::string_view string) -> value<string_tag>;
		static auto make(const environment& env, std::u8string_view string) -> value<string_tag>;
		static auto make(const environment& env, std::u16string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::u8string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::u16string_view string) -> value<string_tag>;
};

class bound_value_for_string : public bound_value_next<string_tag> {
	public:
		using bound_value_next<string_tag>::bound_value_next;
		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

} // namespace js::napi
