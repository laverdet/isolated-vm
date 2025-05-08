module;
#include <cstddef>
#include <cstdint>
#include <string_view>
export module napi_js.primitive;
import isolated_js;
import napi_js.environment;
import napi_js.value;
import nodejs;

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
class bound_value<boolean_tag>
		: public bound_value_next<boolean_tag>,
			public materializable<bound_value<boolean_tag>> {
	public:
		using bound_value_next<boolean_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<bool> tag) const -> bool;
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
};

template <>
class bound_value<number_tag>
		: public bound_value_next<number_tag>,
			public materializable<bound_value<number_tag>> {
	public:
		using bound_value_next<number_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<double> tag) const -> double;
		[[nodiscard]] auto materialize(std::type_identity<int32_t> tag) const -> int32_t;
		[[nodiscard]] auto materialize(std::type_identity<int64_t> tag) const -> int64_t;
		[[nodiscard]] auto materialize(std::type_identity<uint32_t> tag) const -> uint32_t;
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
class bound_value<bigint_tag>
		: public bound_value_next<bigint_tag>,
			public materializable<bound_value<bigint_tag>> {
	public:
		using bound_value_next<bigint_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<bigint> tag) const -> bigint;
		[[nodiscard]] auto materialize(std::type_identity<int64_t> tag) const -> int64_t;
		[[nodiscard]] auto materialize(std::type_identity<uint64_t> tag) const -> uint64_t;
};

// string
template <>
class value<string_tag> : public detail::value_next<string_tag> {
	public:
		using detail::value_next<string_tag>::value_next;
		static auto make(const environment& env, std::string_view string) -> value<string_tag>;
		static auto make(const environment& env, std::u16string_view string) -> value<string_tag>;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(const environment& env, const char string[ Size ]) -> value<string_tag>;
};

template <>
class bound_value<string_tag>
		: public bound_value_next<string_tag>,
			public materializable<bound_value<string_tag>> {
	public:
		using bound_value_next<string_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<std::u16string> tag) const -> std::u16string;
		[[nodiscard]] auto materialize(std::type_identity<std::string> tag) const -> std::string;
};

// ---

template <size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
auto value<string_tag>::make(const environment& env, const char string[ Size ]) -> value<string_tag> {
	return make(env, std::string_view{string, Size});
}
} // namespace js::napi
