module;
#include <array>
#include <concepts>
#include <memory>
#include <string_view>
#include <utility>
export module napi_js:class_;
import :api;
import :callback;
import :external;
import :object;
import :primitive;
import isolated_js;
import ivm.utility;

namespace js::napi {

template <class Type>
class value<class_tag_of<Type>> : public detail::value_next<class_tag_of<Type>> {
	public:
		using detail::value_next<class_tag_of<Type>>::value_next;

		auto construct(auto& env, auto&&... args) const -> value<object_tag>
			requires std::constructible_from<Type, decltype(args)...>;

		template <class Environment>
		static auto make(Environment& env, const auto& class_template) -> value<class_tag_of<Type>>;
};

// ---

template <class Type>
auto value<class_tag_of<Type>>::construct(auto& env, auto&&... args) const -> value<object_tag>
	requires std::constructible_from<Type, decltype(args)...> {
	auto construct = [ & ](napi_value this_arg) -> void {
		// Construct instance through `tagged_external_of<T>` and downcast to `tagged_external`
		auto derived_instance = std::make_unique<tagged_external_of<Type>>(std::forward<decltype(args)>(args)...);
		auto erased_instance = util::safe_pointer_upcast<tagged_external>(std::move(derived_instance));

		// Tag the result
		napi::invoke0(napi_type_tag_object, napi_env{env}, this_arg, &type_tag_for<js::tagged_external>);

		// Wrap w/ finalizer
		apply_finalizer(std::move(erased_instance), [ & ](tagged_external* instance, napi_finalize finalize, void* hint) -> void {
			napi::invoke0(napi_wrap, napi_env{env}, this_arg, instance, finalize, hint, nullptr);
		});
	};

	// Now this gets passed to JavaScript for a moment and hopefully jumps into `construct`
	auto construct_ref = internal_constructor{construct};
	auto* construct_external = napi_value{value<external_tag_of<internal_constructor>>::make(env, &construct_ref)};
	auto* instance = js::napi::invoke(napi_new_instance, napi_env{env}, napi_value{*this}, 1, &construct_external);
	return value<object_tag>::from(instance);
}

template <class Type>
template <class Environment>
auto value<class_tag_of<Type>>::make(Environment& env, const auto& class_template) -> value<class_tag_of<Type>> {
	// Make constructor callback
	auto [ construct_ptr, constructor_data, constructor_finalizer ] =
		make_napi_callback(env, make_constructor_function<Environment, Type>(class_template.constructor.function));
	static_assert(type<decltype(constructor_finalizer)> == type<std::nullptr_t>);

	// Property descriptor creator for methods
	const auto make_method_descriptor =
		[ & ](const auto& property) -> napi_property_descriptor {
		auto name = std::u8string_view{property.name};
		auto [ callback, data, finalizer ] = make_napi_callback(env, make_member_function<Environment, Type>(property.function));
		static_assert(type<decltype(finalizer)> == type<std::nullptr_t>);
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

	// Make class property descriptors
	const auto make_property_descriptor = [ & ](const auto& property) -> napi_property_descriptor {
		switch (property.scope) {
			case class_property_scope::prototype:
				return make_method_descriptor(property);
			case class_property_scope::constructor:
			default:
				std::unreachable();
		}
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
