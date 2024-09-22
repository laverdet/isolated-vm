module;
#include <cstdint>
module ivm.iv8;
import :array;
import v8;

namespace ivm::iv8 {

auto array::Cast(v8::Value* data) -> array* {
	return reinterpret_cast<array*>(v8::Array::Cast(data));
}

auto array::begin(handle_env env, uint32_t& /*length*/) const -> iterator {
	auto* non_const = const_cast<array*>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast)
	return {non_const, env, 0};
}

auto array::end(handle_env env, uint32_t& length) const -> iterator {
	auto* non_const = const_cast<array*>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast)
	return {non_const, env, size(env, length)};
}

auto array::size(handle_env /*env*/, uint32_t& length) const -> uint32_t {
	// nb: `0` means "uninitialized", `length + 1` is stored
	if (length == 0) {
		length = Length() + 1;
	}
	return length - 1;
}

array::iterator::iterator(array* handle, handle_env env, uint32_t index) :
		env{env},
		handle{handle},
		index{index} {}

auto array::iterator::operator*() const -> value_type {
	return iv8::handle{handle->Get(env.context, index).ToLocalChecked(), env};
}

auto array::iterator::operator+=(difference_type offset) -> iterator& {
	index += offset;
	return *this;
}

} // namespace ivm::iv8
