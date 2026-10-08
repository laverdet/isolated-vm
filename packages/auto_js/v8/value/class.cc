export module v8_js:class_definitions;
import :callback;
import :class_;
import :unmaybe;
import :wrappable;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

template <class Type>
template <class... Args>
auto class_template_of<Type>::construct(const auto& lock, Args&&... args) -> v8::Local<v8::Object>
	requires std::constructible_from<Type, Args...> {
	auto instance = unmaybe(this->InstanceTemplate()->NewInstance(lock.context()));
	wrappable_of<Type>::wrap(util::slice(lock), instance, std::forward<Args>(args)...);
	return instance;
}

template <class Type>
auto class_template_of<Type>::make(const auto& lock, const auto& class_template) -> v8::Local<class_template_of> {
	using lock_type = std::remove_cvref_t<decltype(lock)>;
	auto* isolate = lock.isolate();

	// Constructor
	auto [ construct, length ] = make_constructor_function<lock_type, Type>(class_template.constructor.function);
	auto [ constructor_callback, constructor_data ] = make_callback_storage(lock, std::move(construct));
	auto result = v8::FunctionTemplate::New(isolate, constructor_callback, constructor_data, v8::Local<v8::Signature>{}, length, v8::ConstructorBehavior::kAllow);
	constexpr auto latin1_name = util::make_consteval_string_view(util::transcode_string<char>(class_template.constructor.name));
	result->SetClassName(js::transfer_in_strict<v8::Local<v8::String>>(latin1_name, lock));
	auto signature = v8::Signature::New(isolate, result);
	auto prototype = result->PrototypeTemplate();

	// Prototype function templates, with receiver checks
	const auto make_member_template = [ & ](auto function, int length) -> v8::Local<v8::FunctionTemplate> {
		auto [ callback, data ] = make_callback_storage(lock, std::move(function));
		return v8::FunctionTemplate::New(isolate, callback, data, signature, length, v8::ConstructorBehavior::kThrow);
	};

	// Member function property
	const auto define_member_function = [ & ]<class Property>(const Property& property) -> void
		requires(Property::scope == class_property_scope::prototype && Property::disposition == property_disposition::function) {
			constexpr auto latin1_name = util::make_consteval_string_view(util::transcode_string<char>(property.name));
			auto name = js::transfer_in_strict<v8::Local<v8::Name>>(latin1_name, lock);
			auto [ function, length ] = make_member_function<lock_type, Type>(property.function);
			prototype->Set(name, make_member_template(std::move(function), length));
		};

	// Member getter property
	const auto define_member_getter = [ & ]<class Property>(const Property& property) -> void
		requires(Property::scope == class_property_scope::prototype && Property::disposition == property_disposition::accessor) {
			constexpr auto latin1_name = util::make_consteval_string_view(util::transcode_string<char>(property.name));
			auto name = js::transfer_in_strict<v8::Local<v8::Name>>(latin1_name, lock);
			auto [ function, length ] = make_member_function<lock_type, Type>(property.function);
			prototype->SetAccessorProperty(name, make_member_template(std::move(function), length));
		};

	// Static function property
	const auto define_static_function = [ & ]<class Property>(const Property& property) -> void
		requires(Property::scope == class_property_scope::constructor) {
			constexpr auto latin1_name = util::make_consteval_string_view(util::transcode_string<char>(property.name));
			auto name = js::transfer_in_strict<v8::Local<v8::Name>>(latin1_name, lock);
			auto [ function, length ] = make_free_function<lock_type>(property.function);
			auto [ callback, data ] = make_callback_storage(lock, std::move(function));
			result->Set(name, v8::FunctionTemplate::New(isolate, callback, data, v8::Local<v8::Signature>{}, length, v8::ConstructorBehavior::kThrow));
		};

	// Define class properties
	const auto define_property = util::overloaded{
		define_member_function,
		define_member_getter,
		define_static_function,
	};
	const auto [... properties ] = class_template.properties;
	(..., define_property(properties));

	return v8::Local<v8::FunctionTemplate>{result}.As<class_template_of>();
}

} // namespace js::iv8
