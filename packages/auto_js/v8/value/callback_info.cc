module;
#include <compare>
#include <iterator>
#include <ranges>
export module v8_js:callback_info;
import util;
import v8;

namespace js::iv8 {

export class callback_info {
	public:
		class iterator;
		using value_type = v8::Local<v8::Value>;
		explicit callback_info(const v8::FunctionCallbackInfo<v8::Value>& info) :
				info_{&info} {};

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> int { return info_->Length(); }

	private:
		const v8::FunctionCallbackInfo<v8::Value>* info_;
};

class callback_info::iterator : public util::random_access_iterator_facade<int> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = int;
		using value_type = callback_info::value_type;

		iterator() = default;
		iterator(const v8::FunctionCallbackInfo<v8::Value>& info, int index) :
				info_{&info},
				index_{index} {}

		auto operator*() const -> value_type { return (*info_)[ index_ ]; }
		auto operator+=(difference_type offset) -> iterator& {
			index_ += offset;
			return *this;
		}
		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		const v8::FunctionCallbackInfo<v8::Value>* info_{};
		int index_{};
};

// ---

auto callback_info::begin() const -> iterator {
	return iterator{*info_, 0};
};

auto callback_info::end() const -> iterator {
	return iterator{*info_, size()};
};

static_assert(std::ranges::viewable_range<callback_info>);
static_assert(std::random_access_iterator<callback_info::iterator>);

} // namespace js::iv8
