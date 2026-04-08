export module napi_js:external;
import :api;
import :bound_value;
import :object;
import :value_handle;
import std;

namespace js::napi {

class value_for_external : public value_next<external_tag> {
	public:
		// untagged
		static auto make(auto& env, void* pointer) -> value<external_tag>;

		// tagged
		template <class Type>
		static auto make(auto& env, Type* pointer) -> value<external_tag>;
};

class bound_value_for_external : public bound_value_next<external_tag> {
	public:
		using bound_value_next<external_tag>::bound_value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;
};

// ---

auto value_for_external::make(auto& env, void* pointer) -> value<external_tag> {
	auto* external = napi::invoke(napi_create_external, napi_env{env}, pointer, nullptr, nullptr);
	return value<external_tag>::from(external);
}

template <class Type>
auto value_for_external::make(auto& env, Type* pointer) -> value<external_tag> {
	auto external = make(env, static_cast<void*>(pointer));
	napi::invoke0(napi_type_tag_object, napi_env{env}, external, &type_tag_for<Type>);
	return external;
};

template <class Type>
auto bound_value_for_external::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_get_value_external, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
