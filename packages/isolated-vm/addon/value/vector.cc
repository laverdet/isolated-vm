module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.vector;
import :handle.bound_value;
import :handle.value_of;
import :value.object;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

// bound_value<vector_tag>
class EXPORT bound_value_for_vector : public bound_value_next<vector_tag> {
	public:
		class iterator;
		using bound_value_next<vector_tag>::bound_value_next;
		using value_type = value_of<>;

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> std::uint32_t;

	private:
		mutable std::uint32_t size_{};
};

class EXPORT bound_value_for_vector::iterator : public util::random_access_iterator_facade<std::int32_t, std::int64_t> {
	public:
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = std::uint32_t;
		using value_type = bound_value_for_vector::value_type;

		iterator() = default;
		iterator(bound_value_for_vector subject, size_type index);

		auto operator*() const -> value_type;

		constexpr auto operator+=(difference_type offset) -> iterator& {
			index_ += offset;
			return *this;
		}

		constexpr auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		constexpr auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		constexpr auto operator+() const -> size_type { return index_; }

		bound_value_for_vector subject_;
		size_type index_{};
};

} // namespace isolated_vm
