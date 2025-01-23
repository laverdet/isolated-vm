module;
#include <compare>
#include <cstdint>
export module napi_js.array;
import isolated_js;
import ivm.utility;
import napi_js.object;
import napi_js.value;
import napi_js.value.internal;
import nodejs;

namespace js::napi {

template <>
class bound_value<vector_tag> : public bound_value<vector_tag::tag_type> {
	public:
		class iterator;
		using value_type = napi_value;
		bound_value(napi_env env, value<vector_tag> value) :
				bound_value<vector_tag::tag_type>{env, value} {}

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> uint32_t;

	private:
		mutable uint32_t size_{};
};

class bound_value<vector_tag>::iterator : public util::random_access_iterator_facade<iterator, int32_t, int64_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = uint32_t;
		using value_type = bound_value::value_type;

		iterator() :
				subject_{nullptr, value<vector_tag>::from(nullptr)},
				index{} {}
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

		bound_value subject_;
		size_type index{};
};

} // namespace js::napi
