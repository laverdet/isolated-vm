module;
#include <cstdint>
// Fix visibility bug on llvm 18
// /__w/ivm/ivm/packages/nodejs/value/array.cc:9:20: error: missing '#include <napi.h>'; 'Array' must be declared before it is used
//     9 | array::array(Napi::Array array) :
#include <napi.h>
module ivm.node;
import :array;
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
