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
class bound_value<external_tag>
		: public detail::bound_value_next<external_tag>,
			public materializable<bound_value<external_tag>> {
	public:
		using detail::bound_value_next<external_tag>::bound_value_next;
		[[nodiscard]] auto materialize(std::type_identity<void*> tag) const -> void*;
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

		auto operator->() -> auto* { return &value; }

		static auto make(const auto& env, auto&&... args) -> napi_value
			requires std::constructible_from<Type, decltype(args)...>;

	private:
		Type value;
};

// ---

auto bound_value<external_tag>::materialize(std::type_identity<void*> /*tag*/) const -> void* {
	return js::napi::invoke(napi_get_value_external, env(), napi_value{*this});
}

template <class Type>
auto untagged_external<Type>::make(const auto& env, auto&&... args) -> napi_value
	requires std::constructible_from<Type, decltype(args)...> {
	auto instance = std::make_unique<untagged_external>(private_constructor{}, std::forward<decltype(args)>(args)...);
	return js::napi::apply_finalizer(std::move(instance), [ & ](untagged_external* data, napi_finalize finalize, void* hint) {
		return js::napi::invoke(napi_create_external, napi_env{env}, data, finalize, hint);
	});
}

} // namespace js::napi

namespace js {

template <class Type>
struct accept<void, napi::untagged_external<Type>&> {
		auto operator()(external_tag /*tag*/, auto value) const -> napi::untagged_external<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<napi::untagged_external<Type>*>(void_ptr);
		}
};

// nb: Accepting a pointer allows `undefined` to pass
template <class Type>
struct accept<void, napi::untagged_external<Type>*> : accept<void, napi::untagged_external<Type>&> {
		using accept<void, napi::untagged_external<Type>&>::accept;

		auto operator()(external_tag tag, auto&& value) const -> napi::untagged_external<Type>* {
			const accept<void, napi::untagged_external<Type>&>& acceptor = *this;
			return &acceptor(tag, std::forward<decltype(value)>(value));
		}

		auto operator()(undefined_tag /*tag*/, const auto& /*value*/) const -> napi::untagged_external<Type>* {
			return nullptr;
		}
};

} // namespace js
