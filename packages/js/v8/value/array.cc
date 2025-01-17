module;
#include <cstdint>
module v8_js.array;
import v8;

namespace js::iv8 {

auto array::begin() const -> iterator {
	return iterator{*this, context(), 0};
}

auto array::end() const -> iterator {
	return iterator{*this, context(), size()};
}

auto array::size() const -> uint32_t {
	// nb: `0` means "uninitialized", `length + 1` is stored
	if (length_ == 0) {
		length_ = (*this)->Length() + 1;
	}
	return length_ - 1;
}

array::iterator::iterator(v8::Local<v8::Array> array, v8::Local<v8::Context> context, uint32_t index) :
		array_{array},
		context_{context},
		index_{index} {}

auto array::iterator::operator*() const -> value_type {
	return array_->Get(context_, index_).ToLocalChecked();
}

auto array::iterator::operator+=(difference_type offset) -> iterator& {
	index_ += offset;
	return *this;
}

} // namespace js::iv8
