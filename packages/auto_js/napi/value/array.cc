module napi_js;
import :api;
import :bound_value;

namespace js::napi {

auto bound_value_for_vector::begin() const -> iterator {
	return {*this, 0};
}

auto bound_value_for_vector::end() const -> iterator {
	return {*this, size()};
}

auto bound_value_for_vector::size() const -> std::uint32_t {
	if (size_ == 0) {
		size_ = napi::invoke(napi_get_array_length, env(), napi_value{*this}) + 1;
	}
	return size_ - 1;
}

bound_value_for_vector::iterator::iterator(bound_value_for_vector subject, size_type index) :
		subject_{subject},
		index_{index} {}

auto bound_value_for_vector::iterator::operator*() const -> value_type {
	return value_type::from(napi::invoke(napi_get_element, subject_.env(), napi_value{subject_}, index_));
}

} // namespace js::napi
