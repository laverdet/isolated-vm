module;
#include <utility>
export module isolated_js:visit;
import :tag;
import :transfer.types;
import ivm.utility;

namespace js {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		// Swallow `root` argument on behalf of non-meta visitors
		constexpr explicit visit(auto* /*root*/, auto&&... args) :
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

// Returns the key type expected by the delegate (an instance of `visit` or `accept`) target.
export template <util::string_literal Key, class Subject>
struct visit_key_literal {
		constexpr auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept) const -> decltype(auto) {
			return accept(string_tag{}, *this, Key);
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
