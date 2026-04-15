module napi_js;
import :api;
import :array;
import std;

namespace js::napi {

auto bound_value_for_record::into_range() const -> range_type {
	return keys() | std::views::transform(iterator_transform{*this});
}

auto bound_value_for_record::size() const -> std::size_t {
	return keys().size();
}

auto bound_value_for_record::keys() const -> const keys_type& {
	if (napi_value{keys_} == nullptr) {
		auto* property_names = napi::invoke(
			napi_get_all_property_names,
			env(),
			*this,
			napi_key_own_only,
			static_cast<napi_key_filter>(napi_key_enumerable | napi_key_skip_symbols),
			napi_key_keep_numbers
		);
		keys_ = keys_type{env(), js::napi::value<vector_tag>::from(property_names)};
	}
	return keys_;
}

bound_value_for_record::iterator_transform::iterator_transform(const bound_value_for_record& subject) :
		subject_{&subject} {}

auto bound_value_for_record::iterator_transform::operator()(value<value_tag> key) const -> value_type {
	return std::pair{key_type::from(key), subject_->get(key)};
}

} // namespace js::napi
