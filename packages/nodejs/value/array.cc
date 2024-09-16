module;
#include <cstdint>
module ivm.node;
import :array;
import napi;
import v8;

namespace ivm::napi {

array::array(Napi::Array array) :
		array_{array} {}

auto array::begin() const -> iterator {
	return {array_, 0};
}

auto array::end() const -> iterator {
	return {array_, size()};
}

auto array::value() -> Napi::Value& {
	return array_;
}

auto array::size() const -> uint32_t {
	return array_.Length();
}

array::iterator::iterator(Napi::Array array, size_type index) :
		array{array},
		index{index} {}

auto array::iterator::operator*() const -> value_type {
	return array.Get(index);
}

} // namespace ivm::napi
