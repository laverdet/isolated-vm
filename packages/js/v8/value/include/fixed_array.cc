module;
#include <compare>
#include <cstdint>
#include <ranges>
export module ivm.iv8:fixed_array;
import v8;
import :handle;
import ivm.utility;

namespace ivm::js::iv8 {

export class fixed_array : util::non_copyable {
	public:
		class iterator;
		using value_type = v8::Local<v8::Data>;

		fixed_array() = delete;
		fixed_array(v8::Local<v8::FixedArray> array, v8::Local<v8::Context> context);

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> int;

	private:
		v8::Local<v8::FixedArray> array_;
		v8::Local<v8::Context> context_;
};

class fixed_array::iterator : public util::random_access_iterator_facade<iterator, int> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = int;
		using value_type = fixed_array::value_type;

		iterator() = default;
		iterator(v8::Local<v8::FixedArray> array, v8::Local<v8::Context> context, int index);

		auto operator*() const -> value_type;
		auto operator->(this auto& self) { return *self; }
		auto operator+=(difference_type offset) -> iterator&;
		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		v8::Local<v8::FixedArray> array_;
		v8::Local<v8::Context> context_;
		int index_{};
};

static_assert(std::ranges::viewable_range<fixed_array>);
static_assert(std::random_access_iterator<fixed_array::iterator>);

} // namespace ivm::js::iv8
