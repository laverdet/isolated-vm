module;
#include <compare>
#include <cstdint>
export module napi_js:array;
import :bound_value;
import :object;
import :value;
import isolated_js;
import ivm.utility;
import nodejs;

namespace js::napi {

template <>
class bound_value<vector_tag> : public detail::bound_value_next<vector_tag> {
	public:
		class iterator;
		using detail::bound_value_next<vector_tag>::bound_value_next;
		using value_type = value<value_tag>;

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> uint32_t;

	private:
		mutable uint32_t size_{};
};

class bound_value<vector_tag>::iterator : public util::random_access_iterator_facade<int32_t, int64_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = uint32_t;
		using value_type = bound_value::value_type;

		iterator() = default;
		iterator(bound_value subject, size_type index);

		auto operator*() const -> value_type;

		auto operator+=(difference_type offset) -> iterator& {
			index += offset;
			return *this;
		}

		auto operator==(const iterator& right) const -> bool { return index == right.index; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; }

	private:
		auto operator+() const -> size_type { return index; }

		bound_value<vector_tag> subject_;
		size_type index{};
};

} // namespace js::napi
