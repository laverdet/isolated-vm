module;
#include <compare>
#include <cstdint>
#include <ranges>
export module ivm.iv8:array;
import v8;
import :handle;
import ivm.utility;

namespace js::iv8 {

export class array : public v8::Array {
	public:
		class iterator;
		struct handle_data;
		using value_type = v8::Local<v8::Value>;

		[[nodiscard]] auto begin(handle_env env, uint32_t& length) const -> iterator;
		[[nodiscard]] auto end(handle_env env, uint32_t& length) const -> iterator;
		[[nodiscard]] auto size(handle_env env, uint32_t& length) const -> uint32_t;
		static auto Cast(v8::Value* data) -> array*;
};

class array::iterator : public util::random_access_iterator_facade<iterator, int32_t, int64_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = random_access_iterator_facade::difference_type;
		using size_type = uint32_t;
		using value_type = array::value_type;

		iterator() = default;
		iterator(array* array_, handle_env env, uint32_t index);

		auto operator*() const -> value_type;
		auto operator+=(difference_type offset) -> iterator&;
		auto operator==(const iterator& right) const -> bool { return index == right.index; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; }

	private:
		auto operator+() const -> size_type { return index; }

		handle_env env;
		array* array_{};
		uint32_t index{};
};

export using array_handle = handle<array, util::mutable_value<uint32_t>>;
static_assert(std::ranges::range<array_handle>);
static_assert(std::random_access_iterator<array::iterator>);

} // namespace js::iv8
