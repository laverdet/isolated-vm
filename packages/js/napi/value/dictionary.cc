module;
#include <ranges>
#include <utility>
module napi_js;
import :api;
import :array;
import isolated_js;
import ivm.utility;

namespace js::napi {

auto dictionary_like::into_range() const -> range_type {
	if (!keys_) {
		auto property_names = js::napi::invoke(napi_get_property_names, env(), *this);
		keys_ = keys_type{env(), js::napi::value<vector_tag>::from(property_names)};
	}
	return keys_ | std::views::transform(iterator_transform{*this});
}

dictionary_like::iterator_transform::iterator_transform(const dictionary_like& subject) :
		subject_{&subject} {}

auto dictionary_like::iterator_transform::operator()(napi_value key) const -> value_type {
	return std::pair{key, subject_->get(key)};
}

} // namespace js::napi
