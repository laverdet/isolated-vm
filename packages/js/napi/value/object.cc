module napi_js.object;
import ivm.utility;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js::napi {

auto bound_value<object_tag>::get(napi_value key) const -> napi_value {
	return js::napi::invoke(napi_get_property, env(), *this, key);
}

auto bound_value<object_tag>::has(napi_value key) const -> bool {
	return js::napi::invoke(napi_has_own_property, env(), *this, key);
}

auto implementation<object_tag>::set(const environment& env, napi_value key, napi_value value) -> void {
	js::napi::invoke0(napi_set_property, env, *this, key, value);
}

} // namespace js::napi
