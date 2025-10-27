module;
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
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
		static auto make(auto& env, void* pointer) -> value<external_tag>;
};

template <class Type>
class value<external_tag_of<Type>> : public value_next<external_tag_of<Type>> {
	public:
		using value_next<external_tag_of<Type>>::value_next;
		static auto make(auto& env, Type* pointer) -> value<external_tag_of<Type>>;
};

template <>
class bound_value<external_tag> : public bound_value_next<external_tag> {
	public:
		using bound_value_next<external_tag>::bound_value_next;

		[[nodiscard]] explicit operator void*() const;

		template <class Type>
		[[nodiscard]] auto contains(std::type_identity<Type> /*type*/) const -> bool;
};

export template <class Type>
class untagged_external
		: util::non_moveable,
			public util::pointer_facade {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};

	public:
		explicit untagged_external(private_constructor /*private*/, auto&&... args) :
				value{std::forward<decltype(args)>(args)...} {}

		auto operator*() -> Type& { return value; }

		static auto make(const auto& env, auto&&... args) -> napi_value
			requires std::constructible_from<Type, decltype(args)...>;

	private:
		Type value;
};

// ---

auto value<external_tag>::make(auto& env, void* pointer) -> value<external_tag> {
	auto* external = napi::invoke(napi_create_external, napi_env{env}, pointer, nullptr, nullptr);
	return value<external_tag>::from(external);
}

template <class Type>
auto value<external_tag_of<Type>>::make(auto& env, Type* pointer) -> value<external_tag_of<Type>> {
	auto external = value<external_tag>::make(env, pointer);
	napi::invoke0(napi_type_tag_object, napi_env{env}, external, &type_tag_for<Type>);
	return value<external_tag_of<Type>>::from(external);
};

bound_value<external_tag>::operator void*() const {
	return napi::invoke(napi_get_value_external, env(), napi_value{*this});
}

template <class Type>
auto bound_value<external_tag>::contains(std::type_identity<Type> /*type*/) const -> bool {
	return napi::invoke(napi_check_object_type_tag, env(), napi_value{*this}, &type_tag_for<Type>);
}

template <class Type>
auto untagged_external<Type>::make(const auto& env, auto&&... args) -> napi_value
	requires std::constructible_from<Type, decltype(args)...> {
	auto instance = std::make_unique<untagged_external>(private_constructor{}, std::forward<decltype(args)>(args)...);
	return apply_finalizer(
		std::move(instance),
		[ & ](untagged_external* data, napi_finalize finalize, void* hint) -> napi_value {
			return napi::invoke(napi_create_external, napi_env{env}, data, finalize, hint);
		}
	);
}

} // namespace js::napi

namespace js {

template <class Type>
struct accept<void, napi::untagged_external<Type>&> {
		auto operator()(external_tag /*tag*/, visit_holder /*visit*/, auto value) const -> napi::untagged_external<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<napi::untagged_external<Type>*>(void_ptr);
		}
};

// nb: Accepting a pointer allows `undefined` to pass
template <class Type>
struct accept<void, napi::untagged_external<Type>*> : accept<void, napi::untagged_external<Type>&> {
		using accept_type = accept<void, napi::untagged_external<Type>&>;
		using accept_type::accept_type;

		auto operator()(external_tag tag, auto& visit, auto&& value) const -> napi::untagged_external<Type>* {
			return std::addressof(util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(value)>(value)));
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> napi::untagged_external<Type>* {
			return nullptr;
		}
};

} // namespace js
