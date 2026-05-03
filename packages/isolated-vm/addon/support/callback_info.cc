module;
#include "auto_js/export_tag.h"
#include <cassert>
export module isolated_vm:support.callback_info_fwd;
import :handle.value_of;
import auto_js;
import std;
import util;

namespace isolated_vm {

class EXPORT callback_info {
	protected:
		explicit callback_info(const void* info) :
				info_{info} {};

	public:
		class iterator;
		using value_type = value_of<>;

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> int;

	private:
		const void* info_;
};

class callback_info::iterator : public util::random_access_iterator_facade<int> {
	public:
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = int;
		using value_type = callback_info::value_type;

		iterator() = default;
		iterator(const void* info, int index) :
				info_{info},
				index_{index} {}

		auto operator*() const -> value_type;
		auto operator+=(difference_type offset) -> iterator& {
			index_ += offset;
			return *this;
		}
		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		const void* info_{};
		int index_{};
};

// ---

auto callback_info::begin() const -> iterator {
	return iterator{info_, 0};
};

auto callback_info::end() const -> iterator {
	return iterator{info_, size()};
};

static_assert(std::ranges::viewable_range<callback_info>);
static_assert(std::random_access_iterator<callback_info::iterator>);

} // namespace isolated_vm

namespace js {
using namespace isolated_vm;

// `arguments` visitor
template <>
struct visit_subject_for<callback_info> : visit_subject_for<value_of<>> {};

template <class Meta>
struct visit<Meta, callback_info> : visit<Meta, value_of<>> {
		using visit_type = visit<Meta, value_of<>>;
		using visit_type::visit_type;

		using visit_type::operator();

		template <class Accept>
		auto operator()(callback_info info, const Accept& accept) -> accept_target_t<Accept> {
			return accept(vector_tag{}, *this, info);
		}
};

} // namespace js
