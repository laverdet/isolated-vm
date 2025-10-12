module;
#include <cassert>
#include <type_traits>
#include <utility>
export module isolated_js:visit;
import :deferred_receiver;
import :recursive_value;
import :tag;
import :transfer.types;
import ivm.utility;

namespace js {

// `Meta` type for `visit` implementations
export template <class Context, class Subject, class Target, class Reference>
struct visit_meta_holder {
		using accept_property_subject_type = Target;
		using accept_reference_type = Reference;
		using visit_context_type = Context;
		using visit_subject_type = Subject;
};

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
		constexpr auto operator()(auto&& subject, const auto& /*accept*/) const {
			return *std::forward<decltype(subject)>(subject);
		}
};

// `visit` delegated to a sub class passed down from the constructor
export template <class Visit>
struct visit_delegated {
	public:
		using visit_type = Visit;
		constexpr explicit visit_delegated(auto* transfer) : visit_{*transfer} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, Accept& accept) const -> accept_target_t<Accept> {
			return visit_(std::forward<decltype(subject)>(subject), accept);
		}

	private:
		std::reference_wrapper<const visit_type> visit_;
};

// `visit` a possibly recursive subject
export template <class Meta, class Type>
struct visit_maybe_recursive : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;
};

template <class Meta, class Type>
	requires is_recursive_value<Type>
struct visit_maybe_recursive<Meta, Type> : visit_delegated<visit<Meta, Type>> {
		using visit_type = visit_delegated<visit<Meta, Type>>;
		using visit_type::visit_type;
};

// Unfold `Subject` through container visitors.
export template <class Subject>
struct visit_subject_for : std::type_identity<Subject> {};

// Returns the key type expected by the delegate (an instance of `visit` or `accept`) target.
export template <util::string_literal Key, class Subject>
struct visit_key_literal;

template <util::string_literal Key>
struct visit_key_literal<Key, void> {
		template <class Accept>
		constexpr auto operator()(const auto& /*could_be_literally_anything*/, Accept& accept) const -> accept_target_t<Accept> {
			return accept(string_tag{}, *this, Key);
		}
};

// Recursively invoke single-argument `accept` calls until a non-`accept`'able type is returned. `referenceable_value`'s
// are unwrapped and passed to `store`.
template <class Accept>
constexpr auto consume_accept(Accept& /*accept*/, auto&& value, auto /*store*/)
	-> accept_target_t<Accept> {
	return accept_target_t<Accept>(std::forward<decltype(value)>(value));
}

template <class Accept>
constexpr auto consume_accept(Accept& accept, auto&& value, auto /*store*/)
	-> accept_target_t<Accept>
	requires requires { accept(std::declval<decltype(value)>()); } {
	return consume_accept(accept, accept(std::forward<decltype(value)>(value)), util::unused);
}

template <class Accept, class Type>
constexpr auto consume_accept(Accept& accept, js::referenceable_value<Type> value, auto store)
	-> accept_target_t<Accept> {
	store(*value);
	return consume_accept(accept, *std::move(value), util::unused);
}

template <class Accept, class Type, class... Args>
constexpr auto consume_accept(Accept& accept, js::deferred_receiver<Type, Args...> receiver, auto store)
	-> accept_target_t<Accept> {
	store(*receiver);
	std::move(receiver)();
	return consume_accept(accept, *std::move(receiver), util::unused);
}

// Reference map for visitors whose acceptors don't define a `accept_reference_type`
struct null_reference_map {
		explicit null_reference_map(auto&&... /*args*/) {}

		template <class Accept>
		constexpr auto lookup_or_visit(Accept& /*accept*/, auto /*subject*/, auto dispatch) const -> accept_target_t<Accept> {
			return dispatch();
		}

		template <class Accept>
		constexpr auto try_emplace(Accept& accept, auto tag, const auto& visit, auto&& subject) const -> accept_target_t<Accept> {
			return consume_accept(
				accept,
				accept(tag, visit, std::forward<decltype(subject)>(subject)),
				util::unused
			);
		}
};

// Wraps the reference map type (for example `std::unordered_map<napi_value, ...>`) with
// acceptor-aware `try_emplace`.
template <class Map>
struct reference_map_provider {
	public:
		explicit reference_map_provider(auto&&... args) :
				map_{std::forward<decltype(args)>(args)...} {}

		template <class Accept>
		constexpr auto lookup_or_visit(Accept& accept, auto subject, auto dispatch) const -> accept_target_t<Accept> {
			using value_type = accept_target_t<Accept>;
			auto it = map_.find(subject);
			if (it == map_.end()) {
				return dispatch();
			} else {
				return accept.reaccept(std::type_identity<value_type>{}, it->second);
			}
		}

		template <class Accept>
		constexpr auto try_emplace(Accept& accept, auto tag, const auto& visit, auto&& subject) const -> accept_target_t<Accept> {
			return consume_accept(
				*accept,
				accept.accept_direct(tag, visit, std::forward<decltype(subject)>(subject)),
				[ & ](auto&& value) -> void {
					auto [ iterator, inserted ] = map_.try_emplace(subject, std::forward<decltype(value)>(value));
					assert(inserted);
				}
			);
		}

	private:
		mutable Map map_;
};

// Construct a map to store references to visited values. If the acceptor has no reference type then
// `null_reference_map` is used.
template <class Reference, class Map>
struct reference_map;

export template <class Reference, class Map>
using reference_map_t = reference_map<Reference, Map>::type;

template <class Reference, class Map>
struct reference_map : std::type_identity<reference_map_provider<typename Map::template type<Reference>>> {};

template <class Map>
struct reference_map<void, Map> : std::type_identity<null_reference_map> {};

} // namespace js
