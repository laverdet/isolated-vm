module;
#include <ranges>
#include <utility>
module ivm.napi;
import :array;
import :utility;
import :object;
import ivm.js;
import ivm.utility;
import nodejs;

namespace js::napi {

object::object(napi_env env, value<js::object_tag> value) :
		object_like{env, value} {}

auto object::into_range() const -> range_type {
	if (keys_ == array{}) {
		keys_ = array{env(), js::napi::invoke(napi_get_property_names, env(), **this)};
	}
	return keys_ | std::views::transform(iterator_transform{*this});
}

object::iterator_transform::iterator_transform(const object& subject) :
		subject_{&subject} {}

auto object::iterator_transform::operator()(napi_value key) const -> value_type {
	return std::pair{key, subject_->get(key)};
}

} // namespace js::napi
