module;
#include "auto_js/no_unique_address.h"
export module auto_js:accept;
import :transfer.types;
import :visit;
import std;
import util;

namespace js {

// `Meta` type for `accept` implementations
export template <class Wrap, class Context, class Subject>
struct accept_meta_holder {
		using accept_context_type = Context;
		using accept_wrap_type = Wrap;
		using visit_property_subject_type = Subject;
};

// Takes the place of `const auto& /*visit*/` when the visitor is not needed in an acceptor.
// Therefore a register is not wasted on the address. It's probably optimized out anyway since
// almost all methods are inlined but being explicit never hurt anyone.
export struct visit_holder {
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr visit_holder(const auto& /*visit*/) {}
};

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
	private:
		using accept_type = accept<void, Type>;

	public:
		using accept_type::accept_type;
		// Swallow `transfer` argument on behalf of non-meta acceptors
		explicit constexpr accept(auto* /*transfer*/, auto&&... args) :
				accept_type{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `accept` (better error messages)
template <class Type>
struct accept<void, Type>;

// Accept into a forwarded value
template <class Type, class Tag>
struct accept<void, forward<Type, Tag>> {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, std::convertible_to<Type> auto&& subject) const -> forward<Type, Tag> {
			return forward<Type, Tag>{std::forward<decltype(subject)>(subject)};
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// `accept` delegated to a sub class passed down from the constructor
export template <class Accept>
struct accept_delegated;

template <class Accept>
struct accept_target<accept_delegated<Accept>> : accept_target<Accept> {};

template <class Accept>
struct accept_delegated : util::reference_wrapper<Accept> {
		constexpr explicit accept_delegated(auto* transfer) :
				// nb: Casting through pointer is very intentional. Since we inherit from
				// `std::reference_wrapper<T>` casting through a reference might invoke `operator T&` and
				// cause confusing compile-time or run-time errors.
				util::reference_wrapper<Accept>{*static_cast<Accept*>(transfer)} {}

		consteval static auto types(auto recursive) -> auto { return Accept::types(recursive); }
};

// `accept_value_from` is the implementation of `accept_value`. It is an `accept` wrapper which most
// visit instances will see. It is also used in nested container acceptors (`std::pair`,
// `std::vector`, `std::tuple`, etc). It automatically handles unwrapping of referenceable and
// intermediate values, and stores references in the visitors reference map, if there is one
// defined.
template <class Accept>
struct accept_value_from;

template <class Accept>
struct accept_target<accept_value_from<Accept>> : accept_target<Accept> {};

// Recursively invoke single-argument `accept` calls until a non-`accept`'able type is returned.
// `referenceable_value`'s are unwrapped and passed to `insert`. This is tightly coupled to
// `accept_value_from` but split out so that the `reference_of<T>` acceptor can use the consumption
// logic.
export template <class Accept, class Insert>
struct accept_value_direct;

template <class Accept, class Insert>
struct accept_target<accept_value_direct<Accept, Insert>> : accept_target<Accept> {};

// `accept_value_direct`
template <class Accept, class Insert>
struct accept_value_direct {
	private:
		using accept_type = accept_value_from<Accept>;
		using target_type = accept_target_t<Accept>;

	public:
		constexpr accept_value_direct(const accept_type& accept, Insert insert) :
				accept_{accept},
				insert_{std::move(insert)} {}

		constexpr auto operator()(auto tag, auto& visit, auto&& subject) const -> target_type {
			// nb: A `static_assert` is more correct and helpful than a requirement check, since there
			// will be no other acceptors which could do better; if you are invoking an instance of
			// `accept_value` then you expect it to succeed. If the assert fails then something has gone
			// totally off the rails.
			static_assert(std::invocable<const Accept&, decltype(tag), decltype(visit), decltype(subject)>);

			// The `invoke_this_as` call is used by the `js::deferred_receiver`-returning acceptors. They
			// take a `this const auto& self` parameter which must be the `accept_value_from` type and not
			// their own type.
			auto&& result = util::invoke_this_as<Accept>(accept_.get(), tag, visit, std::forward<decltype(subject)>(subject));

			// Insert received value into reference map
			insert_(result);

			// Dispatch `js::deferred_receiver` and convert back into `target_type`
			return make_target(dispatch_referenceable(std::forward<decltype(result)>(result)));
		}

	private:
		constexpr auto make_target(auto&& value) const -> target_type {
			// Recursively invoke single-argument reference casting operation until `target_type` is
			// produced.
			if constexpr (std::invocable<const Accept&, decltype(value)>) {
				return make_target(util::invoke_as<Accept>(accept_.get(), std::forward<decltype(value)>(value)));
			} else {
				return std::forward<decltype(value)>(value);
			}
		}

		util::reference_wrapper<const accept_type> accept_;
		NO_UNIQUE_ADDRESS Insert insert_;
};

// `accept_value_from`
template <class Accept>
struct accept_value_from : Accept {
		using accept_type = Accept;
		using accept_type::accept_type;

		template <class Visit>
		constexpr auto operator()(auto_tag auto tag, Visit& visit, auto&& subject) const -> accept_target_t<accept_type> {
			auto insert = [ & ] -> auto {
				if constexpr (js::has_reference_map(type<Visit>)) {
					return util::overloaded{
						[](const auto& /*value*/) -> void {},
						[ &, subject ]<class Ref>(js::referenceable_value<Ref> value) -> void {
							visit.emplace_subject(subject, *value);
						},
						[ &, subject ]<class Ref, class... Args>(const js::deferred_receiver<Ref, Args...>& receiver) -> void {
							visit.emplace_subject(subject, *receiver);
						},
					};
				} else {
					return [](const auto& /*value*/) -> void {};
				}
			}();
			return accept_value_direct{*this, insert}(tag, visit, std::forward<decltype(subject)>(subject));
		}
};

// `accept_value` is the main utility used by nested containers. See `accept_value_from` notes
// above.
export template <class Meta, class Type>
using accept_value = accept_value_from<typename Meta::accept_wrap_type::template accept<accept<Meta, Type>>>;

// Utility to pass oneself as an object entry acceptor
export template <class First, class Second>
struct accept_entry_pair {
		explicit constexpr accept_entry_pair(auto& self) :
				first{self},
				second{self} {}

		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		First first;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		Second second;
};

template <class First, class Second>
struct accept_target<accept_entry_pair<First, Second>>
		: std::type_identity<std::pair<
				accept_target_t<std::remove_cvref_t<First>>,
				accept_target_t<std::remove_cvref_t<Second>>>> {};

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
export template <class Meta, auto Key, class Type, class Subject>
struct accept_property_value;

// Object key lookup for primitive dictionary variants. This should generally only be used for
// testing, since the subject is basically a C++ heap JSON-ish payload.
template <class Meta, auto Key, class Type>
struct accept_property_value<Meta, Key, Type, void> {
	public:
		explicit constexpr accept_property_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& dictionary) const {
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) -> bool {
				return visit.first(entry.first, first) == util::make_consteval_string_view(Key);
			});
			if (it == dictionary.end()) {
				return second(undefined_in_tag{}, visit, std::monostate{});
			} else {
				return visit.second(it->second, second);
			}
		}

	private:
		accept_value<Meta, std::string> first;
		accept_value<Meta, Type> second;
};

} // namespace js
