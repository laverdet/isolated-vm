module;
#include <compare>
#include <cstdint>
export module napi_js:array;
import :bound_value;
import :object;
import :value_handle;
import auto_js;
import nodejs;
import util;

namespace js::napi {

class bound_value_for_vector : public bound_value_next<vector_tag> {
	public:
		class iterator;
		using bound_value_next<vector_tag>::bound_value_next;
		using value_type = value<value_tag>;

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> uint32_t;

	private:
		mutable uint32_t size_{};
};

class bound_value_for_vector::iterator : public util::random_access_iterator_facade<int32_t, int64_t> {
	public:
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = uint32_t;
		using value_type = bound_value_for_vector::value_type;

		iterator() = default;
		iterator(bound_value_for_vector subject, size_type index);

		auto operator*() const -> value_type;

		auto operator+=(difference_type offset) -> iterator& {
			index_ += offset;
			return *this;
		}

		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		bound_value_for_vector subject_;
		size_type index_{};
};

} // namespace js::napi
