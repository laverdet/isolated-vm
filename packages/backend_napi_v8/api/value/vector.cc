module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;
import v8_js;
import std;

namespace isolated_vm {
using namespace js;

// bound_value_for_vector
auto bound_value_for_vector::begin() const -> iterator {
	return iterator{*this, 0};
}

auto bound_value_for_vector::end() const -> iterator {
	return iterator{*this, size()};
}

auto bound_value_for_vector::size() const -> std::uint32_t {
	if (size_ == 0) {
		size_ = cast_in(value_of{*this})->Length() + 1;
	}
	return size_ - 1;
}

// bound_value_for_vector::iterator
bound_value_for_vector::iterator::iterator(bound_value_for_vector subject, size_type index) :
		subject_{subject},
		index_{index} {}

auto bound_value_for_vector::iterator::operator*() const -> value_type {
	auto context = unwrap_lock_witness(subject_.lock()).context();
	return cast_out(iv8::unmaybe(cast_in(value_of{subject_})->Get(context, index_)));
}

} // namespace isolated_vm
