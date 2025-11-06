module;
#include <array>
#include <concepts>
#include <memory>
#include <string_view>
#include <utility>
export module napi_js:class_definitions;
import :api;
import :callback;
import :class_;
import :external;
import ivm.utility;

namespace js::napi {

template <class Type>
auto value<class_tag_of<Type>>::construct(auto& env, auto&&... args) const -> value<object_tag>
	requires std::constructible_from<Type, decltype(args)...> {
	auto instance = std::make_unique<Type>(std::forward<decltype(args)>(args)...);
	return transfer(env, std::move(instance));
}

template <class Type>
auto value<class_tag_of<Type>>::transfer(auto& env, auto instance) const -> value<object_tag> {
	using element_type = decltype(instance)::element_type;
	auto construct = [ & ](napi_value this_arg) mutable -> napi_value {
		// Tag `this_arg`
		napi::invoke0(napi_type_tag_object, napi_env{env}, this_arg, &type_tag_for<element_type>);

		// Wrap w/ finalizer
		return apply_finalizer(std::move(instance), [ & ](element_type* instance, napi_finalize finalize, void* hint) -> napi_value {
			napi::invoke0(napi_wrap, napi_env{env}, this_arg, instance, finalize, hint, nullptr);
			return this_arg;
		});
	};

	// Now an external (of `internal_constructor`) gets passed to JavaScript for a moment and
	// hopefully it jumps into `construct`
	auto construct_ref = internal_constructor{construct};
	auto* construct_external = napi_value{value<external_tag>::make(env, &construct_ref)};
	auto* this_arg = napi::invoke(napi_new_instance, napi_env{env}, napi_value{*this}, 1, &construct_external);
	return value<object_tag>::from(this_arg);
}

template <class Type>
template <class Environment>
auto value<class_tag_of<Type>>::make(Environment& env, const auto& class_template) -> value<class_tag_of<Type>> {
	// Make constructor callback
	auto [ construct_ptr, constructor_data ] =
		make_callback_storage(env, make_constructor_function<Environment, Type>(class_template.constructor.function));
	static_assert(std::remove_cvref_t<decltype(class_template.constructor)>::disposition == property_disposition::function);
	static_assert(!requires { typename decltype(constructor_data)::element_type; });

	// Member function property descriptor
	const auto make_member_function_descriptor = [ & ]<class Property>(const Property& property) -> napi_property_descriptor
		requires(Property::scope == class_property_scope::prototype) {
			auto name = std::u8string_view{property.name};
			auto [ callback, data ] = make_callback_storage(env, make_member_function<Environment, Type>(property.function));
			static_assert(!requires { typename decltype(data)::element_type; });
			return {
				.utf8name = reinterpret_cast<const char*>(name.data()),
				.name{},

				.method = callback,
				.getter{},
				.setter{},
				.value{},

				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				.attributes = static_cast<napi_property_attributes>(napi_writable | napi_configurable),
				.data = data,
			};
		};

	// Static function property descriptor
	const auto make_static_function_descriptor = [ & ]<class Property>(const Property& property) -> napi_property_descriptor
		requires(Property::scope == class_property_scope::constructor) {
			auto name = std::u8string_view{property.name};
			auto [ callback, data ] = make_callback_storage(env, make_free_function<Environment>(property.function));
			static_assert(!requires { typename decltype(data)::element_type; });
			return {
				.utf8name = reinterpret_cast<const char*>(name.data()),
				.name{},

				.method = callback,
				.getter{},
				.setter{},
				.value{},

				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				.attributes = static_cast<napi_property_attributes>(napi_static | napi_writable | napi_configurable),
				.data = data,
			};
		};

	// Make class property descriptors
	const auto make_property_descriptor = util::overloaded{
		make_member_function_descriptor,
		make_static_function_descriptor,
	};
	const auto [... properties ] = class_template.properties;
	const auto property_descriptors = std::array<napi_property_descriptor, sizeof...(properties)>{
		make_property_descriptor(properties)...
	};

	// Finally, define the class
	auto name = std::u8string_view{class_template.constructor.name};
	auto result = napi::invoke(
		napi_define_class,
		napi_env{env},
		reinterpret_cast<const char*>(name.data()),
		name.length(),
		construct_ptr,
		constructor_data,
		property_descriptors.size(),
		property_descriptors.data()
	);
	return js::napi::value<class_tag_of<Type>>::from(result);
}

} // namespace js::napi
