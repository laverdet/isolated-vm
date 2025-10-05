module;
#include <ranges>
#include <utility>
export module isolated_js:accept;
import :transfer.types;
import :visit;
import ivm.utility;

namespace js {

// `Meta` type for `accept` implementations
export template <class Wrap, class Context, class Subject, class Reference>
struct accept_meta_holder {
		using accept_context_type = Context;
		using accept_wrap_type = Wrap;
		using visit_property_subject_type = Subject;
		using visit_subject_reference = Reference;
};

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

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

// `accept` with transfer wrapping
export template <class Meta, class Type>
using accept_next = Meta::accept_wrap_type::template accept<accept<Meta, Type>>;

// Extract the target type from an `accept`-like type. This is used to know what to cast the result
// of `accept()` to.
template <class Type>
struct accept_target;

export template <class Type>
using accept_target_t = accept_target<Type>::type;

template <class Accept>
struct accept_target : std::type_identity<typename Accept::accept_target_type> {};

template <class Meta, class Type>
struct accept_target<accept<Meta, Type>> : std::type_identity<Type> {};

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
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) {
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

// Construct a map to store references to visited values. If the visitor has no reference types then
// there can be no map.
template <class Reference, template <class> class Map>
struct reference_map;

struct null_reference_map {
		explicit null_reference_map(auto&&... /*args*/) {}
};

export template <class Reference, template <class> class Map>
using reference_map_t = reference_map<Reference, Map>::type;

template <class Reference, template <class> class Map>
struct reference_map : std::type_identity<Map<Reference>> {};

template <template <class> class Map>
struct reference_map<void, Map> : std::type_identity<null_reference_map> {};

} // namespace js
