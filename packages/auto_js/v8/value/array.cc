module v8_js;
import util;
import v8;

namespace js::iv8 {

auto value_for_array::begin() const -> iterator {
	return iterator{util::slice(*this), context(), 0};
}

auto value_for_array::end() const -> iterator {
	return iterator{util::slice(*this), context(), size()};
}

auto value_for_array::size() const -> std::uint32_t {
	// nb: `0` means "uninitialized", `length + 1` is stored
	if (length_ == 0) {
		length_ = (*this)->Length() + 1;
	}
	return length_ - 1;
}

value_for_array::iterator::iterator(v8::Local<v8::Array> array, v8::Local<v8::Context> context, std::uint32_t index) :
		array_{array},
		context_{context},
		index_{index} {}

auto value_for_array::iterator::operator*() const -> value_type {
	return iv8::unmaybe(array_->Get(context_, index_));
}

auto value_for_array::iterator::operator+=(difference_type offset) -> iterator& {
	index_ += offset;
	return *this;
}

} // namespace js::iv8
