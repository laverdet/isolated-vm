export module napi_js:primitive;
import :api;
import :bound_value;
import :environment_fwd;
import :value_handle;
import std;

namespace js::napi {

// boolean
class bound_value_for_boolean : public bound_value_next<boolean_tag> {
	public:
		using bound_value_next<boolean_tag>::bound_value_next;
		[[nodiscard]] explicit operator bool() const;
};
// number
class bound_value_for_number : public bound_value_next<number_tag> {
	public:
		using bound_value_next<number_tag>::bound_value_next;
		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator std::int32_t() const;
		[[nodiscard]] explicit operator std::int64_t() const;
		[[nodiscard]] explicit operator std::uint32_t() const;
};

// bigint
class bound_value_for_bigint : public bound_value_next<bigint_tag> {
	public:
		using bound_value_next<bigint_tag>::bound_value_next;
		[[nodiscard]] explicit operator bigint() const;
		[[nodiscard]] explicit operator std::int64_t() const;
		[[nodiscard]] explicit operator std::uint64_t() const;
};

// string
class bound_value_for_string : public bound_value_next<string_tag> {
	public:
		using bound_value_next<string_tag>::bound_value_next;
		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

} // namespace js::napi
