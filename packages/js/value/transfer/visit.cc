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

// `Meta` type for `visit` implementations
export template <class Context, class Subject, class Target>
struct visit_meta_holder {
		using accept_property_subject_type = Target;
		using visit_context_type = Context;
		using visit_subject_type = Subject;
};

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

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

// Directly return forwarded value
template <class Type>
struct visit<void, forward<Type>> {
		constexpr auto operator()(auto&& value, const auto& /*accept*/) const {
			return *std::forward<decltype(value)>(value);
		}
};

// Unfold `Subject` through container visitors.
export template <class Subject>
struct visit_subject_for : std::type_identity<Subject> {};

// Takes the place of `const auto& /*visit*/` when the visitor is not needed in an acceptor.
// Therefore a register is not wasted on the address. It's probably optimized out anyway since
// almost all methods are inlined but being explicit never hurt anyone.
export struct visit_holder {
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr visit_holder(const auto& /*visit*/) {}
};

// Returns the key type expected by the delegate (an instance of `visit` or `accept`) target.
export template <util::string_literal Key, class Subject>
struct visit_key_literal;

template <util::string_literal Key>
struct visit_key_literal<Key, void> {
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
