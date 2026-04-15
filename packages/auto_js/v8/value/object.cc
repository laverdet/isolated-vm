module v8_js;
import :array;
import :handle;
import :lock;
import :primitive;
import std;
import v8;

namespace js::iv8 {

constexpr auto get_property(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::Name> key) -> v8::Local<v8::Value> {
	// Maybe I'm misunderstanding the "without causing side effects" part of `GetRealNamedProperty`'s
	// documentation, but it definitely invokes getters.
	return iv8::unmaybe(object->GetRealNamedProperty(context, key));
}

constexpr auto get_property(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::Number> key) -> v8::Local<v8::Value> {
	return iv8::unmaybe(object->Get(context, key.As<v8::Uint32>()->Value()));
}

constexpr auto get_property(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::Primitive> key) -> v8::Local<v8::Value> {
	// Weird `if` here since there is a "quick" `IsString()` internally, but not for name, number, or
	// symbol.
	if (key->IsString() || !key->IsNumber()) {
		return get_property(context, object, key.As<v8::Name>());
	} else {
		return get_property(context, object, key.As<v8::Number>());
	}
}

auto object::keys() const -> const array& {
	if (keys_ == array{}) {
		auto property_names = (*this)->GetPropertyNames(
			context(),
			v8::KeyCollectionMode::kOwnOnly,
			static_cast<v8::PropertyFilter>(v8::PropertyFilter::ONLY_ENUMERABLE | v8::PropertyFilter::SKIP_SYMBOLS),
			v8::IndexFilter::kIncludeIndices,
			v8::KeyConversionMode::kKeepNumbers
		);
		keys_ = array{witness(), iv8::unmaybe(property_names)};
	}
	return keys_;
}

auto object::get(v8::Local<v8::Name> key) -> v8::Local<v8::Value> {
	return get_property(context(), util::slice(*this), key);
}

auto object::get(v8::Local<v8::Number> key) -> v8::Local<v8::Value> {
	return get_property(context(), util::slice(*this), key);
}

auto object::get(v8::Local<v8::Primitive> key) -> v8::Local<v8::Value> {
	return get_property(context(), util::slice(*this), key);
}

auto object::into_range() -> range_type {
	// NOLINTNEXTLINE(cppcoreguidelines-slicing)
	return keys() | std::views::transform(iterator_transform{*this, context()});
}

auto object::size() const -> std::size_t {
	return keys().size();
}

object::iterator_transform::iterator_transform(
	v8::Local<v8::Object> object,
	v8::Local<v8::Context> context
) :
		object_{object},
		context_{context} {}

auto object::iterator_transform::operator()(v8::Local<v8::Value> key) const -> value_type {
	auto value = get_property(context_, object_, key.As<v8::Primitive>());
	return std::pair{key.As<v8::Primitive>(), value};
}

} // namespace js::iv8
