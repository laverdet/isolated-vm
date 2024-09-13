module;
#include <sys/types.h>
#include <compare>
#include <iterator>
export module ivm.node:arguments;
import napi;
import ivm.utility;

namespace ivm::napi {

export class arguments {
	public:
		class iterator;
		using value_type = Napi::Value;

		explicit arguments(const Napi::CallbackInfo& info);

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> size_t;

	private:
		const Napi::CallbackInfo* info;
};

class arguments::iterator : public arithmetic_facade<iterator, ssize_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = size_t;
		using value_type = arguments::value_type;

		iterator() = default;
		iterator(const Napi::CallbackInfo& info, size_type index);

		auto operator*() const -> value_type;
		auto operator->() const -> value_type { return **this; }
		auto operator[](difference_type offset) const -> value_type { return *(*this + offset); }

		auto operator+=(difference_type offset) -> iterator& {
			index += offset;
			return *this;
		}

		auto operator==(const iterator& right) const -> bool { return index == right.index; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; };

	private:
		auto operator+() const -> ssize_t { return static_cast<ssize_t>(index); }

		const Napi::CallbackInfo* info{};
		size_type index{};
};

static_assert(std::ranges::range<arguments>);
static_assert(std::random_access_iterator<arguments::iterator>);

} // namespace ivm::napi
