module;
#include "auto_js/no_unique_address.h"
export module auto_js:accept;
import :visit;
import std;
import util;

namespace js {

// `Meta` type for `accept` implementations
template <class Wrap, class Subject>
struct accept_meta_holder {
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

// Default `accept` selection
export struct select_default_accept {
		template <class Meta, class Type>
		using accept = js::accept<Meta, Type>;
};

// Top-level acceptor configuration
export template <class Target, class... Args>
struct select_accept : select_default_accept {
		using target_type = Target;
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

		constexpr static auto accept_tags_of() { return std::tuple<Tag>{}; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// `accept` delegated to a related sub class passed down from the constructor
export template <class Accept>
struct accept_proxy_relative;

template <class Accept>
struct accept_target<accept_proxy_relative<Accept>> : accept_target<Accept> {};

template <class Accept>
struct accept_proxy_relative : util::reference_wrapper<Accept> {
		constexpr explicit accept_proxy_relative(auto* transfer) :
				// nb: Casting through pointer is very intentional. Since we inherit from
				// `std::reference_wrapper<T>` casting through a reference might invoke `operator T&` and
				// cause confusing compile-time or run-time errors.
				util::reference_wrapper<Accept>{*static_cast<Accept*>(transfer)} {}

		constexpr static auto accept_tags_of() { return js::accept_tags_of_v<Accept>; }
		consteval static auto types(auto recursive) -> auto { return Accept::types(recursive); }
};

// Acceptor wrapper which declares `accept_target_type`
export template <class Target, class Accept>
struct accept_as : Accept {
		using accept_target_type = Target;
		using Accept::Accept;
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

		constexpr static auto accept_allows_throw = js::accept_allows_throw<Accept>;

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
		constexpr auto operator()(auto tag, Visit& visit, auto&& subject) const -> accept_target_t<accept_type> {
			auto insert = [ & ] -> auto {
				if constexpr (js::visit_with_reference_map<Visit>) {
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

// `accept_value_from` around an explicit acceptor, wrapped by `Meta`'s wrap type.
export template <class Meta, class Accept>
using accept_value_with = accept_value_from<typename Meta::accept_wrap_type::template accept<Accept>>;

// `accept_value` is the main utility used by nested containers.
export template <class Meta, class Type>
using accept_value = accept_value_with<Meta, accept<Meta, Type>>;

// `accept_value` for a top-level acceptos, via `select_accept`
export template <class Meta, class Type, class... Args>
using select_accept_value = accept_value_with<Meta, typename select_accept<std::remove_cvref_t<Type>, Args...>::template accept<Meta, Type>>;

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

		constexpr auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& subject) const {
			auto it = std::ranges::find_if(subject, [ & ](const auto& entry) -> bool {
				return visit.first(entry.first, first) == util::make_consteval_string_view(Key);
			});
			if (it == subject.end()) {
				return second(undefined_in_tag{}, visit, std::monostate{});
			} else {
				return visit.second(it->second, second);
			}
		}

	private:
		accept_value<Meta, std::string> first;
		accept_value<Meta, Type> second;
};

// Composition of acceptor delegates
export template <class... Delegates>
class acceptor_delegate_of {
	public:
		constexpr explicit acceptor_delegate_of(Delegates... delegates) : delegates_{std::move(delegates)...} {}

		constexpr auto operator()(auto&& subject, const auto& lock) const {
			using result_type = decltype(std::get<0>(delegates_)(std::forward<decltype(subject)>(subject), lock));
			return util::template_traverse(
				util::sequence_cw<sizeof...(Delegates)>,
				util::overloaded{
					[ & ](auto ii, auto next) -> result_type {
						if (auto result = std::get<ii>(delegates_)(std::forward<decltype(subject)>(subject), lock)) {
							return result;
						}
						return next();
					},
					[] -> result_type { return std::nullopt; },
				}
			);
		}

	private:
		std::tuple<Delegates...> delegates_;
};

template <class Type>
struct is_acceptor_delegate : std::bool_constant<false> {};

template <class... Delegates>
struct is_acceptor_delegate<acceptor_delegate_of<Delegates...>> : std::bool_constant<true> {};

export template <class Type>
concept acceptor_delegate_type = is_acceptor_delegate<Type>::value;

} // namespace js
