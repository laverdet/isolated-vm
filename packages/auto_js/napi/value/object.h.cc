export module napi_js:object;
import :primitive;
import std;

namespace js::napi {

// object
class local_for_object : public local_next<object_tag> {
	public:
		auto assign(const auto& lock, auto source) const -> void;
};

class value_for_object : public value_next<object_tag> {
	public:
		using value_next<object_tag>::value_next;

		template <class Type>
		[[nodiscard]] auto try_cast(std::type_identity<Type> /*type*/) const -> Type*;

		[[nodiscard]] auto get(napi_value key) const -> local_of<>;
		[[nodiscard]] auto has(napi_value key) const -> bool;

		auto set(napi_value key, napi_value value) -> void;
};

// date
class value_for_date : public value_next<date_tag> {
	public:
		using value_next<date_tag>::value_next;
		[[nodiscard]] explicit operator js_clock::time_point() const;
};

// ---

template <class Type>
auto value_for_object::try_cast(std::type_identity<Type> /*type*/) const -> Type* {
	if (napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>)) {
		return static_cast<Type*>(napi::invoke(napi_unwrap, env(), napi_value{*this}));
	} else {
		return nullptr;
	}
}

} // namespace js::napi
