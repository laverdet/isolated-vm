module;
#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>
export module napi_js.object;
import isolated_js;
import napi_js.environment;
import napi_js.primitive;
import napi_js.value;
import napi_js.value.internal;
import nodejs;

namespace js::napi {

template <>
class bound_value<object_tag> : public bound_value<object_tag::tag_type> {
	public:
		bound_value(napi_env env, value<object_tag> value) :
				bound_value<object_tag::tag_type>{env, value} {}

		[[nodiscard]] auto get(napi_value key) const -> napi_value;
		[[nodiscard]] auto has(napi_value key) const -> bool;
};

template <>
struct implementation<object_tag> : implementation<object_tag::tag_type> {
		template <class... Entries>
		auto assign(const auto_environment auto& env, std::tuple<Entries...> entries) -> void;

		auto set(const environment& env, napi_value key, napi_value value) -> void;
};

// ---

template <class... Entries>
auto implementation<object_tag>::assign(const auto_environment auto& env, std::tuple<Entries...> entries) -> void {
	std::invoke(
		[ & ]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) {
			return (invoke(std::integral_constant<size_t, Index>{}), ...);
		},
		[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
			set(
				env,
				transfer_in_strict<napi_value>(std::get<Index>(entries).first, env),
				transfer_in_strict<napi_value>(std::get<Index>(entries).second, env)
			);
		},
		std::index_sequence_for<Entries...>{}
	);
}

} // namespace js::napi
