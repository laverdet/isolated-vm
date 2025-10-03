module;
#include <concepts>
#include <type_traits>
#include <utility>
export module isolated_js:visit;
import :deferred_receiver;
import :tag;
import :transfer.types;
import ivm.utility;

namespace js {

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		// Swallow `transfer` argument on behalf of non-meta visitors
		constexpr explicit visit(auto* /*transfer*/, auto&&... args) :
				visit<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `visit` (better error messages)
template <class Type>
struct visit<void, Type>;

// Takes the place of `const auto& /*visit*/` when the visitor is not needed in an acceptor.
// Therefore a register is not wasted on the address. It's probably optimized out anyway since
// almost all methods are inlined but being explicit never hurt anyone.
export struct visit_holder {
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr visit_holder(const auto& /*visit*/) {}
};

// `invoke_accept` helpers
template <class Accept>
constexpr auto unwrap_accepted(const Accept& /*accept*/, auto&& result, const auto& /*visit*/, const auto& /*subject*/) -> accept_target_t<Accept> {
	return accept_target_t<Accept>(std::forward<decltype(result)>(result));
}

template <class Accept, class Type, class... Args>
constexpr auto unwrap_accepted(const Accept& /*accept*/, js::referenceable_value<Type> value, const auto& /*visit*/, const auto& /*subject*/) -> accept_target_t<Accept> {
	return accept_target_t<Accept>(*value);
}

template <class Accept, class Type, class... Args>
constexpr auto unwrap_accepted(Accept& accept, js::deferred_receiver<Type, Args...> receiver, const auto& visit, auto&& subject) -> accept_target_t<Accept> {
	std::move(receiver)(accept, visit, std::forward<decltype(subject)>(subject));
	return accept_target_t<Accept>(*receiver);
}

// Invoked by `visit` implementations to cast the result of `accept(...)` to the appropriate type.
// This allows acceptors to return values which are convertible to the accepted type, for example
// `v8::Local<v8::String>` -> `v8::Local<v8::Value>`.
export template <class Accept>
constexpr auto invoke_accept(Accept& accept, auto tag, const auto& visit, auto&& value) -> accept_target_t<Accept> {
	return unwrap_accepted(
		accept,
		accept(tag, visit, std::forward<decltype(value)>(value)),
		visit,
		std::forward<decltype(value)>(value)
	);
}

// Returns the key type expected by the delegate (an instance of `visit` or `accept`) target.
export template <util::string_literal Key, class Subject>
struct visit_key_literal {
		constexpr auto operator()(const auto& /*could_be_literally_anything*/, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, string_tag{}, *this, Key);
		}
};

// Forward cast operators to the underlying method `materialize(std::type_identity<To>, ...)`
export template <class Type>
class materializable {
	protected:
		friend Type;
		materializable() = default;

	public:
		template <class To>
		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] operator To(this auto&& self)
			requires requires {
				{ self.materialize(std::type_identity<To>{}) } -> std::same_as<To>;
			} {
			return self.materialize(std::type_identity<To>{});
		}
};

} // namespace js
