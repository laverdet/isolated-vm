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

} // namespace ivm::iv8
