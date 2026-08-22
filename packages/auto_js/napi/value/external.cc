export module napi_js:external;
import :object;
import std;

namespace js::napi {

class local_for_external : public local_next<external_tag> {
	public:
		// untagged
		static auto make(environment_lock_witness lock, void* pointer) -> local_of<external_tag>;

		// tagged
		template <class Type>
		static auto make(environment_lock_witness lock, Type* pointer) -> local_of<external_tag>;
};

class value_for_external : public value_next<external_tag> {
	public:
		using value_next<external_tag>::value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;
};

// ---

auto local_for_external::make(environment_lock_witness lock, void* pointer) -> local_of<external_tag> {
	auto* external = napi::invoke(napi_create_external, napi_env{lock}, pointer, nullptr, nullptr);
	return local_of<external_tag>::from(external);
}

template <class Type>
auto local_for_external::make(environment_lock_witness lock, Type* pointer) -> local_of<external_tag> {
	auto external = make(lock, static_cast<void*>(pointer));
	napi::invoke0(napi_type_tag_object, napi_env{lock}, external, &type_tag_for<Type>);
	return external;
};

template <class Type>
auto value_for_external::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_get_value_external, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
