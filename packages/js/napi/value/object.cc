module;
#include <type_traits>
module napi_js.object;
import ivm.utility;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js::napi {

// object
auto bound_value<object_tag>::get(napi_value key) const -> napi_value {
	return js::napi::invoke(napi_get_property, env(), napi_value{*this}, key);
}

auto bound_value<object_tag>::has(napi_value key) const -> bool {
	return js::napi::invoke(napi_has_own_property, env(), napi_value{*this}, key);
}

auto bound_value<object_tag>::set(napi_value key, napi_value value) -> void {
	js::napi::invoke0(napi_set_property, env(), napi_value{*this}, key, value);
}

// date
auto value<date_tag>::make(const environment& env, js_clock::time_point date) -> value<date_tag> {
	return value<date_tag>::from(napi::invoke(napi_create_date, env, date.time_since_epoch().count()));
}

auto bound_value<date_tag>::materialize(std::type_identity<js_clock::time_point> /*tag*/) const -> js_clock::time_point {
	return js_clock::time_point{js_clock::duration{napi::invoke(napi_get_date_value, env(), napi_value{*this})}};
}

} // namespace js::napi
