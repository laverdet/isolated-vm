module;
#include "auto_js/chunk_view.h"
module v8_js;
import :map;
import std;
import v8;

namespace js::iv8 {

auto value_for_map::entries() const -> const value_for_array& {
	if (entries_ == value_for_array{}) {
		entries_ = value_for_array{context(), (*this)->AsArray(), false};
	}
	return entries_;
}

auto value_for_map::into_range() -> range_type {
	return entries() | std::views::chunk(2) | std::views::transform(iterator_transform{});
}

auto value_for_map::size() const -> std::size_t {
	return (*this)->Size();
}

auto value_for_map::iterator_transform::operator()(std::ranges::range_reference_t<entries_view> entry) const -> value_type {
	auto iterator = entry.begin();
	auto key = *iterator;
	return {key, *++iterator};
}

} // namespace js::iv8
