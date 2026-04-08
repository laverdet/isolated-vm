export module napi_js:object;
import :api;
import :bound_value;
import :environment_fwd;
import :primitive;
import :value_handle;
import std;

namespace js::napi {

// object
class value_for_object : public value_next<object_tag> {
	public:
		auto assign(this value<object_tag> self, auto_environment auto& env, auto source) -> void;
};

class bound_value_for_object : public bound_value_next<object_tag> {
	public:
		using bound_value_next<object_tag>::bound_value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;

		[[nodiscard]] auto get(napi_value key) const -> value<value_tag>;
		[[nodiscard]] auto has(napi_value key) const -> bool;

		auto set(napi_value key, napi_value value) -> void;
};

// date
class value_for_date : public value_next<date_tag> {
	public:
		static auto make(const environment& env, js_clock::time_point date) -> value<date_tag>;
};

class bound_value_for_date : public bound_value_next<date_tag> {
	public:
		using bound_value_next<date_tag>::bound_value_next;
		[[nodiscard]] explicit operator js_clock::time_point() const;
};

// ---

template <class Type>
auto bound_value_for_object::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_unwrap, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
