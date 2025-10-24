module;
#include <concepts>
#include <memory>
export module napi_js:external;
import :api;
import :bound_value;
import :finalizer;
import :value;
import isolated_js;
import ivm.utility;

namespace js::napi {

template <>
class bound_value<external_tag> : public detail::bound_value_next<external_tag> {
	public:
		using detail::bound_value_next<external_tag>::bound_value_next;
		[[nodiscard]] explicit operator void*() const;
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

bound_value<external_tag>::operator void*() const {
	return js::napi::invoke(napi_get_value_external, env(), napi_value{*this});
}

template <class Type>
auto untagged_external<Type>::make(const auto& env, auto&&... args) -> napi_value
	requires std::constructible_from<Type, decltype(args)...> {
	auto instance = std::make_unique<untagged_external>(private_constructor{}, std::forward<decltype(args)>(args)...);
	return js::napi::apply_finalizer(
		std::move(instance),
		[ & ](untagged_external* data, napi_finalize finalize, void* hint) -> auto {
			return js::napi::invoke(napi_create_external, napi_env{env}, data, finalize, hint);
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
