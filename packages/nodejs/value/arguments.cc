module;
#include <sys/types.h>
#include <compare>
#include <iterator>
module ivm.node;
import :arguments;
import napi;

namespace ivm::napi {

arguments::arguments(const Napi::CallbackInfo& info) :
		info{&info} {}

auto arguments::begin() const -> iterator {
	return {*info, 0};
}

auto arguments::end() const -> iterator {
	return {*info, info->Length()};
}

auto arguments::size() const -> size_t {
	return info->Length();
}

arguments::iterator::iterator(const Napi::CallbackInfo& info, size_t index) :
		info{&info},
		index{index} {}

auto arguments::iterator::operator*() const -> value_type {
	return (*info)[ index ];
}

auto arguments::iterator::operator+=(difference_type offset) -> iterator& {
	index += offset;
	return *this;
}

auto arguments::iterator::operator++(int) -> iterator {
	auto result = *this;
	++*this;
	return result;
}

auto arguments::iterator::operator--(int) -> iterator {
	auto result = *this;
	--*this;
	return result;
}

auto arguments::iterator::operator-(const iterator& right) const -> difference_type {
	return static_cast<difference_type>(index) - static_cast<difference_type>(right.index);
}

} // namespace ivm::napi
