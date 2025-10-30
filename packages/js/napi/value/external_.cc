module;
#include <type_traits>
export module napi_js:external;
import :api;
import :bound_value;
import :value_handle;
import ivm.utility;

namespace js::napi {

template <>
class value<external_tag> : public value_next<external_tag> {
	public:
		using value_next<external_tag>::value_next;

		// untagged
		static auto make(auto& env, void* pointer) -> value<external_tag>;

		// tagged
		template <class Type>
		static auto make(auto& env, Type* pointer) -> value<external_tag>;
};

template <>
class bound_value<external_tag> : public bound_value_next<external_tag> {
	public:
		using bound_value_next<external_tag>::bound_value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;
};

// ---

auto value<external_tag>::make(auto& env, void* pointer) -> value<external_tag> {
	auto* external = napi::invoke(napi_create_external, napi_env{env}, pointer, nullptr, nullptr);
	return value<external_tag>::from(external);
}

template <class Type>
auto value<external_tag>::make(auto& env, Type* pointer) -> value<external_tag> {
	auto external = make(env, static_cast<void*>(pointer));
	napi::invoke0(napi_type_tag_object, napi_env{env}, external, &type_tag_for<Type>);
	return external;
};

template <class Type>
auto bound_value<external_tag>::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_get_value_external, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
