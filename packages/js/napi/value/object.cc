module;
#include <ranges>
#include <string>
module ivm.napi;
import :array;
import :utility;
import :object;
import ivm.utility;
import napi;

namespace ivm::napi {

auto object::into_range() const -> range_type {
	if (keys_ == array{}) {
		keys_ = array{env(), ivm::napi::invoke(napi_get_property_names, env(), **this)};
	}
	return keys_ | std::views::transform(iterator_transform{*this});
}

auto object::get(napi_value key) const -> napi_value {
	return ivm::napi::invoke(napi_get_property, env(), *this, key);
}

auto object::has(napi_value key) const -> bool {
	return ivm::napi::invoke(napi_has_own_property, env(), *this, key);
}

object::iterator_transform::iterator_transform(const object& subject) :
		subject_{&subject} {}

auto object::iterator_transform::operator()(napi_value key) const -> value_type {
	return std::pair{key, subject_->get(key)};
}

} // namespace ivm::napi
