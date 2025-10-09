module;
#include <tuple>
#include <utility>
export module napi_js:object;
import :bound_value;
import :environment;
import :primitive;
import :value;
import isolated_js;
import nodejs;

namespace js::napi {

// object
template <>
class value<object_tag> : public detail::value_next<object_tag> {
	public:
		using detail::value_next<object_tag>::value_next;

		template <class... Entries>
		auto assign(auto_environment auto& env, std::tuple<Entries...> entries) -> void;
};

template <>
class bound_value<object_tag> : public detail::bound_value_next<object_tag> {
	public:
		using detail::bound_value_next<object_tag>::bound_value_next;

		[[nodiscard]] auto get(napi_value key) const -> value<value_tag>;
		[[nodiscard]] auto has(napi_value key) const -> bool;

		auto set(napi_value key, napi_value value) -> void;
};

// date
template <>
class value<date_tag> : public detail::value_next<date_tag> {
	public:
		using detail::value_next<date_tag>::value_next;
		static auto make(const environment& env, js_clock::time_point date) -> value<date_tag>;
};

template <>
class bound_value<date_tag>
		: public detail::bound_value_next<date_tag>,
			public materializable<bound_value<date_tag>> {
	public:
		using detail::bound_value_next<date_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<js_clock::time_point> tag) const -> js_clock::time_point;
};

// ---

template <class... Entries>
auto value<object_tag>::assign(auto_environment auto& env, std::tuple<Entries...> entries) -> void {
	bound_value value{env, *this};
	const auto [... indices ] = util::sequence<sizeof...(Entries)>;
	(..., [ & ]() constexpr {
		auto& entry_ref = std::get<indices>(entries);
		auto entry_js_val = js::transfer_in_strict<std::array<napi_value, 2>>(
			std::tuple{std::move(entry_ref.first), std::move(entry_ref.second)},
			env
		);
		value.set(entry_js_val[ 0 ], entry_js_val[ 1 ]);
	}());
}

} // namespace js::napi
