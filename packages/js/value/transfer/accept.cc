module;
#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
export module isolated_js:accept;
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
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, std::convertible_to<Type> auto&& subject) const -> forward<Type, Tag> {
			return forward<Type, Tag>{std::forward<decltype(subject)>(subject)};
		}
};

// `accept` delegated to a sub class passed down from the constructor
export template <class Accept>
struct accept_delegated_from;

template <class Accept>
struct accept_target<accept_delegated_from<Accept>> : accept_target<Accept> {};

template <class Accept>
struct accept_delegated_from : std::reference_wrapper<Accept> {
		constexpr explicit accept_delegated_from(auto* transfer) :
				std::reference_wrapper<Accept>{*transfer} {}
};

// Recursively invoke single-argument `accept` calls until a non-`accept`'able type is returned.
// `referenceable_value`'s are unwrapped and passed to `insert`.
export template <class Accept, class Insert>
struct accept_store_unwrapped;

template <class Accept, class Insert>
struct accept_target<accept_store_unwrapped<Accept, Insert>> : accept_target<Accept> {};

template <class Accept, class Insert>
struct accept_store_unwrapped {
	private:
		using accept_type = std::remove_cvref_t<decltype(*std::declval<const Accept&>())>;
		using target_type = accept_target_t<Accept>;

	public:
		constexpr accept_store_unwrapped(const Accept& accept, Insert insert) :
				accept_{accept},
				insert_{std::move(insert)} {}

		constexpr auto operator()(auto tag, auto& visit, auto&& subject) const -> target_type
			requires std::invocable<const accept_type&, decltype(tag), decltype(visit), decltype(subject)> {
			return consume(util::invoke_as<accept_type>(accept_.get(), tag, visit, std::forward<decltype(subject)>(subject)));
		}

	private:
		[[nodiscard]] constexpr auto consume(target_type /*&&*/ value) const -> target_type {
			return std::forward<decltype(value)>(value);
		}

		constexpr auto consume(auto&& value) const -> target_type
			requires std::invocable<const accept_type&, decltype(value)> {
			return consume((*accept_.get())(std::forward<decltype(value)>(value)));
		}

		template <class Type>
		[[nodiscard]] constexpr auto consume(js::referenceable_value<Type> value) const -> target_type {
			insert_(*value);
			return consume(*std::move(value));
		}

		template <class Type, class... Args>
		[[nodiscard]] constexpr auto consume(js::deferred_receiver<Type, Args...> receiver) const -> target_type {
			insert_(*receiver);
			std::move(receiver)();
			return consume(*std::move(receiver));
		}

		std::reference_wrapper<const Accept> accept_;
		[[no_unique_address]] Insert insert_;
};

// `accept` wrapper which is passed to visit instances. It is also used in nested container
// acceptors (`std::pair`, `std::vector`, `std::tuple`, etc). It automatically handles unwrapping of
// referenceable and intermediate values, and stores references in the visitors reference map, if
// there is one defined.
template <class Accept>
struct accept_value_from_direct;

template <class Accept>
struct accept_target<accept_value_from_direct<Accept>> : accept_target<Accept> {};

template <class Accept>
struct accept_value_from_direct : Accept {
		using accept_type = Accept;
		using accept_type::accept_type;

		constexpr auto operator*() const -> const accept_type& { return *this; }

		// forward reference provider
		constexpr auto operator()(auto type, auto&& value) const -> type_t<type> {
			return util::invoke_as<Accept>(*this, type, std::forward<decltype(value)>(value));
		}

		// nb: `std::invocable<accept_type, ...>` causes a circular requirement. I think it's fine to
		// leave it out though since `accept_value_from` is a terminal acceptor.
		constexpr auto operator()(auto_tag auto tag, auto& visit, auto&& subject) const -> accept_target_t<accept_type> {
			auto insert = [ & ]() -> auto {
				if constexpr (requires { visit.has_reference_map; }) {
					return [ subject = subject, &visit = visit ](const auto& value) { visit.emplace_subject(subject, value); };
				} else {
					return util::unused;
				}
			}();
			return accept_store_unwrapped{*this, insert}(tag, visit, std::forward<decltype(subject)>(subject));
		}
};

template <class Meta, class Accept>
using accept_value_from = accept_value_from_direct<typename Meta::accept_wrap_type::template accept<Accept>>;

export template <class Meta, class Type>
using accept_value = accept_value_from<Meta, accept<Meta, Type>>;

// `accept_value_recursive` is a utility on top of `accept_value` which detects recursive types and
// uses a delegating acceptor for those.
template <class Meta, class Type>
struct accept_value_recursive_type;

export template <class Meta, class Type>
using accept_value_recursive = accept_value_recursive_type<Meta, Type>::type;

template <class Meta, class Type>
struct accept_value_recursive_type
		: std::type_identity<accept_value_from<Meta, accept<Meta, Type>>> {};

template <class Meta, class Type>
	requires Type::is_recursive_type
struct accept_value_recursive_type<Meta, Type>
		: std::type_identity<accept_value_from<Meta, accept_delegated_from<accept<Meta, Type>>>> {};

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
				using value_type = decltype(Key)::value_type;
				using character_type = std::remove_extent_t<std::remove_cvref_t<value_type>>;
				return visit.first(entry.first, first) == std::basic_string_view<character_type>{Key};
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
