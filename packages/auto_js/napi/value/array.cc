module napi_js;

namespace js::napi {

auto value_for_vector::begin() const -> iterator {
	return {*this, 0};
}

auto value_for_vector::end() const -> iterator {
	return {*this, size()};
}

auto value_for_vector::size() const -> std::uint32_t {
	if (size_ == 0) {
		size_ = napi::invoke(napi_get_array_length, env(), napi_value{*this}) + 1;
	}
	return size_ - 1;
}

value_for_vector::iterator::iterator(value_for_vector subject, size_type index) :
		subject_{subject},
		index_{index} {}

auto value_for_vector::iterator::operator*() const -> value_type {
	auto lock = subject_.lock();
	auto* subject = napi_value{subject_};
	if (subject_.maybe_sparse_ && !fast_has_element(lock, subject, index_)) {
		throw js::type_error{u"Sparse arrays are not supported"};
	}
	return value_type::from(fast_get_element(lock, subject, index_));
}

} // namespace js::napi
