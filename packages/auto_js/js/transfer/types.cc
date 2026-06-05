export module auto_js:transfer.types;
import :tag;
import std;
import util;

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
		forward() = default;

		template <class From, std::convertible_to<Tag> FromTag>
		// NOLINTNEXTLINE(google-explicit-constructor)
		forward(forward<From, FromTag> from) :
				value_{*std::move(from)} {}
		explicit forward(std::convertible_to<Type> auto&& value, Tag /*tag*/ = {}) :
				value_{std::forward<decltype(value)>(value)} {}

		constexpr auto operator*(this auto&& self) -> auto&& { return std::forward<decltype(self)>(self).value_; }

	private:
		Type value_;
};

template <class Type>
forward(Type) -> forward<Type>;

template <class Type, class Tag>
forward(Type, Tag) -> forward<Type, Tag>;

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

// Check if the given visit type has a reference map
template <class Visit>
concept visit_with_reference_map = std::remove_cvref_t<Visit>::visit_with_reference_map();

// Prevents runtime throws via `transfer_strict`
template <class Accept>
concept accept_allows_throw = std::remove_cvref_t<Accept>::accept_allows_throw;

// Extract `accept_tags_of` from an acceptor
export template <class Accept>
constexpr auto accept_tags_of_v = std::tuple{};

export template <class Accept>
	requires requires { Accept::accept_tags_of(); }
constexpr auto accept_tags_of_v<Accept> = Accept::accept_tags_of();

} // namespace js
