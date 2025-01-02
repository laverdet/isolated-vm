module ivm.napi;
import :object_like;
import :utility;
import :value_of;
import ivm.utility;
import napi;

namespace ivm::js::napi {

auto object_like::get(napi_value key) const -> napi_value {
	return js::napi::invoke(napi_get_property, env(), *this, key);
}

auto object_like::has(napi_value key) const -> bool {
	return js::napi::invoke(napi_has_own_property, env(), *this, key);
}

} // namespace ivm::js::napi
