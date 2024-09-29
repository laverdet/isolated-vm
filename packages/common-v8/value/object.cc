module;
#include <ranges>
#include <string>
module ivm.iv8;
import :array;
import :handle;
import :object;
import :string;
import ivm.utility;
import v8;

namespace ivm::iv8 {

auto object::Cast(v8::Value* data) -> object* {
	return reinterpret_cast<object*>(v8::Object::Cast(data));
}

auto object::get(handle_env env, array_handle& /*keys*/, std::string_view key) -> v8::Local<v8::Value> {
	auto key_string = string::make(env.isolate, key);
	return this->GetRealNamedProperty(env.context, key_string.As<v8::Name>()).ToLocalChecked();
}

auto object::into_range(handle_env env, array_handle& keys) -> range_type {
	if (keys == array_handle{}) {
		auto property_names = this->GetPropertyNames(
			env.context,
			v8::KeyCollectionMode::kOwnOnly,
			v8::PropertyFilter::ONLY_ENUMERABLE,
			v8::IndexFilter::kIncludeIndices,
			v8::KeyConversionMode::kConvertToString
		);
		keys = array_handle{property_names.ToLocalChecked().As<iv8::array>(), env, 0};
	}
	return keys | std::views::transform(iterator_transform{env, this});
}

object::iterator_transform::iterator_transform(handle_env env, v8::Object* object) :
		env{env}, object{object} {}

auto object::iterator_transform::operator()(v8::Local<v8::Value> key) const -> value_type {
	auto value = object->GetRealNamedProperty(env.context, key.As<v8::Name>()).ToLocalChecked();
	return std::pair{key, value};
}

} // namespace ivm::iv8
