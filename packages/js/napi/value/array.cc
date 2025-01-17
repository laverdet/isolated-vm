module;
#include <cstdint>
module napi_js.array;
import napi_js.utility;
import nodejs;

namespace js::napi {

auto array::begin() const -> iterator {
	return {*this, 0};
}

auto array::end() const -> iterator {
	return {*this, size()};
}

auto array::size() const -> uint32_t {
	if (size_ == 0) {
		size_ = js::napi::invoke(napi_get_array_length, env(), **this) + 1;
	}
	return size_ - 1;
}

array::iterator::iterator(array subject, size_type index) :
		subject_{subject},
		index{index} {}

auto array::iterator::operator*() const -> value_type {
	return js::napi::invoke(napi_get_element, subject_.env(), subject_, index);
}

} // namespace js::napi
