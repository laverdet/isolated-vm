module;
#include <algorithm>
#include <string>
#include <utility>
export module isolated_js:accept;
import :recursive_value;
import :transfer.types;
import :visit;
import ivm.utility;

namespace js {

// `Meta` type for `accept` implementations
export template <class Wrap, class Context, class Subject>
struct accept_meta_holder {
		using accept_context_type = Context;
		using accept_wrap_type = Wrap;
		using visit_property_subject_type = Subject;
};

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
		// Swallow `transfer` argument on behalf of non-meta acceptors
		explicit constexpr accept(auto* /*transfer*/, auto&&... args) :
				accept<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `accept` (better error messages)
template <class Type>
struct accept<void, Type>;

// Accept into a forwarded value
template <class Type, class Tag>
struct accept<void, forward<Type, Tag>> {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, std::convertible_to<Type> auto&& value) const -> forward<Type, Tag> {
			return forward<Type, Tag>{std::forward<decltype(value)>(value)};
		}
};

// `accept` delegated to a sub class passed down from the constructor
export template <class Accept>
struct accept_delegated {
	public:
		using accept_target_type = accept_target_t<Accept>;
		using accept_type = Accept;
		constexpr explicit accept_delegated(auto* transfer) : accept_{*transfer} {}

		constexpr auto operator()(auto tag, const auto& visit, auto&& value) const -> accept_target_type
			requires std::invocable<accept_type&, decltype(tag), decltype(visit), decltype(value)> {
			return accept_(tag, visit, std::forward<decltype(value)>(value));
		}

	private:
		std::reference_wrapper<accept_type> accept_;
};

// `accept` with transfer wrapping
export template <class Meta, class Type>
struct accept_next;

template <class Meta, class Type>
struct accept_target_of<accept_next<Meta, Type>> : std::type_identity<Type> {};

template <class Meta, class Type>
struct accept_next : Meta::accept_wrap_type::template accept<accept<Meta, Type>> {
		using accept_type = Meta::accept_wrap_type::template accept<accept<Meta, Type>>;
		using accept_type::accept_type;
};

// `accept` possibly a possible recursive target
export template <class Meta, class Type>
struct accept_recursive_next : accept_next<Meta, Type> {
		using accept_target_type = accept_target_t<accept_next<Meta, Type>>;
		using accept_type = accept_next<Meta, Type>;
		using accept_type::accept_type;
};

template <class Meta, class Type>
	requires is_recursive_value<Type>
struct accept_recursive_next<Meta, Type> : accept_delegated<accept_next<Meta, Type>> {
		using accept_target_type = Type;
		using accept_type = accept_delegated<accept_next<Meta, Type>>;
		using accept_type::accept_type;
};

// Specialized by certain containers to map `Target` to the first meaningful acceptor. This is
// specifically used with `accept_property_subject_type` in relation to property names.
export template <class Target>
struct accept_target_for : std::type_identity<Target> {};

// Specialized to map the given `accept` "target" to the "subject" type it expects for visited
// properties. For primitive types this is `void`. This corresponds to the `Subject` parameter of
// `accept_property_value` and `visit_key_literal`. This is only used for template specialization.
export template <class Target>
struct accept_property_subject;

template <class Type>
using accept_property_subject_t = accept_property_subject<Type>::type;

template <class Type>
struct accept_property_subject : std::type_identity<void> {};

// Returns the value corresponding to a key with an accepted object subject.
export template <class Meta, util::string_literal Key, class Type, class Subject>
struct accept_property_value;

// Object key lookup for primitive dictionary variants. This should generally only be used for
// testing, since the subject is basically a C++ heap JSON-ish payload.
template <class Meta, util::string_literal Key, class Type>
struct accept_property_value<Meta, Key, Type, void> {
	public:
		explicit constexpr accept_property_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		constexpr auto operator()(dictionary_tag /*tag*/, const auto& visit, const auto& dictionary) {
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) -> bool {
				return visit.first(entry.first, first) == Key;
			});
			if (it == dictionary.end()) {
				return second(undefined_in_tag{}, visit, std::monostate{});
			} else {
				return visit.second(it->second, second);
			}
		}

	private:
		accept_next<Meta, std::string> first;
		accept_next<Meta, Type> second;
};

} // namespace js
