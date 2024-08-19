module;
#include <compare>
#include <cstdint>
#include <ranges>
export module ivm.v8:array;
import v8;
import :handle;
import :utility;
import ivm.utility;

namespace ivm::iv8 {

export class array : public v8::Array {
	public:
		class iterator;
		struct handle_data;
		using value_type = handle<v8::Value>;

		auto begin(handle_env env, uint32_t& length) const -> iterator;
		auto end(handle_env env, uint32_t& length) const -> iterator;
		auto size(handle_env env, uint32_t& length) const -> uint32_t;
		static auto Cast(v8::Value* data) -> array*;
};

class array::iterator {
	public:
		using value_type = array::value_type;
		using difference_type = int32_t;

		iterator() = default;
		iterator(array* handle, handle_env env, uint32_t index);

		auto operator*() const -> value_type;
		auto operator->() const -> value_type { return **this; }
		auto operator[](difference_type offset) const -> value_type { return *(*this + offset); }

		auto operator+=(difference_type offset) -> iterator&;
		auto operator++() -> iterator& { return *this += 1; }
		auto operator++(int) -> iterator;
		auto operator--() -> iterator& { return *this -= 1; }
		auto operator--(int) -> iterator;
		auto operator-=(difference_type offset) -> iterator& { return *this += -offset; }
		auto operator+(difference_type offset) const -> iterator { return iterator{*this} += offset; }
		auto operator-(difference_type offset) const -> iterator { return iterator{*this} -= offset; }
		auto operator-(const iterator& right) const -> difference_type;
		friend auto operator+(difference_type left, const iterator& right) -> iterator { return right + left; }

		auto operator==(const iterator& right) const -> bool { return index == right.index; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; }

	private:
		handle_env env;
		array* handle{};
		uint32_t index{};
};

export using array_handle = handle<array, mutable_value<uint32_t>>;
static_assert(std::ranges::range<array_handle>);
static_assert(std::random_access_iterator<array::iterator>);

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
	return iv8::handle{unmaybe(handle->Get(env.context, index)), env};
}

auto array::iterator::operator+=(difference_type offset) -> iterator& {
	index += offset;
	return *this;
}

auto array::iterator::operator++(int) -> iterator {
	auto result = *this;
	++*this;
	return result;
}

auto array::iterator::operator--(int) -> iterator {
	auto result = *this;
	--*this;
	return result;
}

auto array::iterator::operator-(const iterator& right) const -> difference_type {
	return static_cast<int32_t>(int64_t{index} - int64_t{right.index});
}

} // namespace ivm::iv8
