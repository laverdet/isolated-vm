module napi_js;

namespace js::napi {

// object
auto value_for_object::get(napi_value key) const -> local_of<> {
	return local_of<>::from(napi::invoke(napi_get_property, env(), napi_value{*this}, key));
}

auto value_for_object::has(napi_value key) const -> bool {
	return napi::invoke(napi_has_own_property, env(), napi_value{*this}, key);
}

auto value_for_object::set(napi_value key, napi_value value) -> void {
	napi::invoke0(napi_set_property, env(), napi_value{*this}, key, value);
}

// date
value_for_date::operator js_clock::time_point() const {
	return js_clock::time_point{js_clock::duration{napi::invoke(napi_get_date_value, env(), napi_value{*this})}};
}

} // namespace js::napi
