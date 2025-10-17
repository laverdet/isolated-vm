module;
#include <cstddef>
#include <cstdint>
#include <string_view>
export module napi_js:primitive;
import :api;
import :bound_value;
import :environment;
import :value;
import isolated_js;

namespace js::napi {

// undefined
template <>
class value<undefined_tag> : public detail::value_next<undefined_tag> {
	public:
		using value_next<undefined_tag>::value_next;
		static auto make(const environment& env) -> value<undefined_tag>;
		static auto make(const environment& env, auto&& /*undefined*/) -> value<undefined_tag> {
			return make(env);
		}
};

// null
template <>
class value<null_tag> : public detail::value_next<null_tag> {
	public:
		using detail::value_next<null_tag>::value_next;
		static auto make(const environment& env) -> value<null_tag>;
		static auto make(const environment& env, auto&& /*null*/) -> value<null_tag> {
			return make(env);
		}
};

// boolean
template <>
class value<boolean_tag> : public detail::value_next<boolean_tag> {
	public:
		using value_next<boolean_tag>::value_next;
		static auto make(const environment& env, bool boolean) -> value<boolean_tag>;
};

template <>
class bound_value<boolean_tag> : public detail::bound_value_next<boolean_tag> {
	public:
		using detail::bound_value_next<boolean_tag>::bound_value_next;
		[[nodiscard]] explicit operator bool() const;
};

// number
template <>
class value<number_tag> : public detail::value_next<number_tag> {
	public:
		using detail::value_next<number_tag>::value_next;
		static auto make(const environment& env, double number) -> value<number_tag>;
		static auto make(const environment& env, int32_t number) -> value<number_tag>;
		static auto make(const environment& env, int64_t number) -> value<number_tag>;
		static auto make(const environment& env, uint32_t number) -> value<number_tag>;

		static auto make_property_name(const environment& env, int32_t number) -> value<number_tag> { return make(env, number); }
};

template <class Numeric>
class value<number_tag_of<Numeric>> : public detail::value_next<number_tag_of<Numeric>> {
	public:
		using detail::value_next<number_tag_of<Numeric>>::value_next;
		static auto make(const environment& env, Numeric number) -> value<number_tag_of<Numeric>> {
			return value<number_tag_of<Numeric>>::from(value<number_tag>::make(env, number));
		}
};

template <>
class bound_value<number_tag> : public detail::bound_value_next<number_tag> {
	public:
		using detail::bound_value_next<number_tag>::bound_value_next;
		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator int32_t() const;
		[[nodiscard]] explicit operator int64_t() const;
		[[nodiscard]] explicit operator uint32_t() const;
};

// bigint
template <>
class value<bigint_tag> : public detail::value_next<bigint_tag> {
	public:
		using detail::value_next<bigint_tag>::value_next;
		static auto make(const environment& env, const bigint& number) -> value<bigint_tag>;
		static auto make(const environment& env, int64_t number) -> value<bigint_tag>;
		static auto make(const environment& env, uint64_t number) -> value<bigint_tag>;
};

template <>
class bound_value<bigint_tag> : public detail::bound_value_next<bigint_tag> {
	public:
		using detail::bound_value_next<bigint_tag>::bound_value_next;
		[[nodiscard]] explicit operator bigint() const;
		[[nodiscard]] explicit operator int64_t() const;
		[[nodiscard]] explicit operator uint64_t() const;
};

// string
template <>
class value<string_tag> : public detail::value_next<string_tag> {
	public:
		using detail::value_next<string_tag>::value_next;
		static auto make(const environment& env, std::string_view string) -> value<string_tag>;
		static auto make(const environment& env, std::u8string_view string) -> value<string_tag>;
		static auto make(const environment& env, std::u16string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::u8string_view string) -> value<string_tag>;
		static auto make_property_name(const environment& env, std::u16string_view string) -> value<string_tag>;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(const environment& env, const char string[ Size ]) -> value<string_tag>;
};

template <>
class bound_value<string_tag> : public detail::bound_value_next<string_tag> {
	public:
		using detail::bound_value_next<string_tag>::bound_value_next;
		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

// ---

template <size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
auto value<string_tag>::make(const environment& env, const char string[ Size ]) -> value<string_tag> {
	return napi::invoke(napi_create_string_utf8, napi_env{env}, string, Size);
}

} // namespace js::napi
