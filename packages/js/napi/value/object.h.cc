module;
#include <array>
#include <tuple>
#include <utility>
export module napi_js:object;
import :api;
import :bound_value;
import :environment_fwd;
import :primitive;
import :value_handle;

namespace js::napi {

// object
template <>
class value<object_tag> : public value_next<object_tag> {
	public:
		using value_next<object_tag>::value_next;

		template <class... Entries>
		auto assign(auto_environment auto& env, std::tuple<Entries...> entries) -> void;
};

template <>
class bound_value<object_tag> : public bound_value_next<object_tag> {
	public:
		using bound_value_next<object_tag>::bound_value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;

		[[nodiscard]] auto get(napi_value key) const -> value<value_tag>;
		[[nodiscard]] auto has(napi_value key) const -> bool;

		auto set(napi_value key, napi_value value) -> void;
};

// date
template <>
class value<date_tag> : public value_next<date_tag> {
	public:
		using value_next<date_tag>::value_next;
		static auto make(const environment& env, js_clock::time_point date) -> value<date_tag>;
};

template <>
class bound_value<date_tag> : public bound_value_next<date_tag> {
	public:
		using bound_value_next<date_tag>::bound_value_next;
		[[nodiscard]] explicit operator js_clock::time_point() const;
};

// ---

template <class... Entries>
auto value<object_tag>::assign(auto_environment auto& env, std::tuple<Entries...> entries) -> void {
	bound_value value{env, *this};
	auto& [... entries_n ] = entries;
	(..., [ & ]() constexpr {
		auto [ first, second ] = js::transfer_in_strict<std::array<napi_value, 2>>(
			std::tuple{std::move(entries_n).first, std::move(entries_n).second},
			env
		);
		value.set(first, second);
	}());
}

template <class Type>
auto bound_value<object_tag>::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_unwrap, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
