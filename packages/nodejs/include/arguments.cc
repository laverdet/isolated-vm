module;
#include <sys/types.h>
#include <compare>
#include <iterator>
export module ivm.node:arguments;
import napi;

namespace ivm::napi {

export class arguments {
	public:
		class iterator;
		using value_type = Napi::Value;

		explicit arguments(const Napi::CallbackInfo& info);

		auto begin() const -> iterator;
		auto end() const -> iterator;
		auto size() const -> size_t;

	private:
		const Napi::CallbackInfo* info;
};

class arguments::iterator {
	public:
		using value_type = arguments::value_type;
		using difference_type = ssize_t;

		iterator() = default;
		iterator(const Napi::CallbackInfo& info, size_t index);

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
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; };

	private:
		const Napi::CallbackInfo* info{};
		size_t index{};
};

static_assert(std::ranges::range<arguments>);
static_assert(std::random_access_iterator<arguments::iterator>);

} // namespace ivm::napi
