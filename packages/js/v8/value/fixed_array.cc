module v8_js;
import v8;

namespace js::iv8 {

fixed_array::fixed_array(v8::Local<v8::Context> context, v8::Local<v8::FixedArray> array) :
		array_{array},
		context_{context} {}

auto fixed_array::begin() const -> iterator {
	return {array_, context_, 0};
}

auto fixed_array::end() const -> iterator {
	return {array_, context_, size()};
}

auto fixed_array::size() const -> int {
	return array_->Length();
}

fixed_array::iterator::iterator(v8::Local<v8::FixedArray> array, v8::Local<v8::Context> context, int index) :
		array_{array},
		context_{context},
		index_{index} {}

auto fixed_array::iterator::operator*() const -> value_type {
	return array_->Get(context_, index_);
}

auto fixed_array::iterator::operator+=(difference_type offset) -> iterator& {
	index_ += offset;
	return *this;
}

} // namespace js::iv8
