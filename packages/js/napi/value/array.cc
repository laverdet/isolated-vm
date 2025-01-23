module;
#include <cstdint>
module napi_js.array;
import napi_js.utility;
import nodejs;

namespace js::napi {

auto bound_value<vector_tag>::begin() const -> iterator {
	return {*this, 0};
}

auto bound_value<vector_tag>::end() const -> iterator {
	return {*this, size()};
}

auto bound_value<vector_tag>::size() const -> uint32_t {
	if (size_ == 0) {
		size_ = js::napi::invoke(napi_get_array_length, env(), *this) + 1;
	}
	return size_ - 1;
}

bound_value<vector_tag>::iterator::iterator(bound_value subject, size_type index) :
		subject_{subject},
		index{index} {}

auto bound_value<vector_tag>::iterator::operator*() const -> value_type {
	return js::napi::invoke(napi_get_element, subject_.env(), subject_, index);
}

} // namespace js::napi
