module;
#include <ranges>
#include <string>
module v8_js;
import :array;
import :handle;
import :lock;
import :string;
import ivm.utility;
import v8;

namespace js::iv8 {

auto object::get(v8::Local<v8::Value> key) -> v8::Local<v8::Value> {
	return (*this)->GetRealNamedProperty(context(), key.As<v8::Name>()).ToLocalChecked();
}

auto object::into_range() -> range_type {
	if (keys_ == array{}) {
		auto property_names = (*this)->GetPropertyNames(
			context(),
			v8::KeyCollectionMode::kOwnOnly,
			v8::PropertyFilter::ONLY_ENUMERABLE,
			v8::IndexFilter::kIncludeIndices,
			v8::KeyConversionMode::kConvertToString
		);
		keys_ = array{witness(), property_names.ToLocalChecked()};
	}
	return keys_ | std::views::transform(iterator_transform{*this, context()});
}

object::iterator_transform::iterator_transform(
	v8::Local<v8::Object> object,
	v8::Local<v8::Context> context
) :
		object_{object},
		context_{context} {}

auto object::iterator_transform::operator()(v8::Local<v8::Value> key) const -> value_type {
	auto value = object_->GetRealNamedProperty(context_, key.As<v8::Name>()).ToLocalChecked();
	return std::pair{key, value};
}

} // namespace js::iv8
