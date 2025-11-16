module;
#include <utility>
export module isolated_js:transfer.types;
import ivm.utility;
import :tag;

namespace js {

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

// Allows the subject or target of `transfer` to pass through a value directly without invoking
// `visit` or `accept`. For example, as a directly created element of an array.
export template <class Type>
struct forward_tag_for : std::type_identity<value_tag> {};

template <class Type>
using forward_tag_for_t = forward_tag_for<Type>::type;

export template <class Type, class Tag = forward_tag_for_t<Type>>
struct forward : util::pointer_facade {
	public:
		explicit forward(const Type& value, Tag /*tag*/ = {}) :
				value_{value} {}
		explicit forward(Type&& value, Tag /*tag*/ = {}) :
				value_{std::move(value)} {}

		constexpr auto operator*(this auto&& self) -> auto&& { return std::forward<decltype(self)>(self).value_; }

	private:
		Type value_;
};

// Extract the target type from an `accept`-like type. This is used to know what to cast the result
// of `accept()` to.
template <class Type>
struct accept_target;

export template <class Type>
using accept_target_t = accept_target<Type>::type;

template <class Accept>
	requires requires { typename Accept::accept_target_type; }
struct accept_target<Accept> : std::type_identity<typename Accept::accept_target_type> {};

template <class Meta, class Type>
struct accept_target<accept<Meta, Type>> : std::type_identity<Type> {};

// Override to delegate transfer behavior for a compatible type
export template <class Type>
struct transfer_type;

template <class Type>
using transfer_type_t = transfer_type<Type>::type;

template <class Type>
	requires requires { typename Type::transfer_type; }
struct transfer_type<Type> : std::type_identity<typename Type::transfer_type> {};

// Check if the given type has a reference map
constexpr auto has_reference_map = []<class Type>(std::type_identity<Type> /*type*/) consteval {
	using visit_type = std::remove_cvref_t<Type>;
	if constexpr (requires { visit_type::has_reference_map; }) {
		return visit_type::has_reference_map;
	} else {
		return false;
	}
};

} // namespace js
