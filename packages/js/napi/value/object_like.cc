module napi_js.object_like;
import ivm.utility;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js::napi {

auto object_like::get(napi_value key) const -> napi_value {
	return js::napi::invoke(napi_get_property, env(), *this, key);
}

auto object_like::has(napi_value key) const -> bool {
	return js::napi::invoke(napi_has_own_property, env(), *this, key);
}

auto object_like::set(napi_value key, napi_value value) -> void {
	js::napi::invoke0(napi_set_property, env(), *this, key, value);
}

} // namespace js::napi
