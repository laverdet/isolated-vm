module;
#include <cassert>
#include <concepts>
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
		constexpr auto operator()(auto&& value, const auto& /*accept*/) const {
			return *std::forward<decltype(value)>(value);
		}
};

// `visit` delegated to a sub class passed down from the constructor
export template <class Visit>
struct visit_delegated {
	public:
		using visit_type = Visit;
		constexpr explicit visit_delegated(auto* transfer) : visit_{*transfer} {}

		constexpr auto operator()(auto&& subject, auto& accept) const -> decltype(auto) {
			return visit_(std::forward<decltype(subject)>(subject), accept);
		}

	private:
		std::reference_wrapper<const visit_type> visit_;
};

// `visit` a possibly recursive subject
export template <class Meta, class Type>
struct visit_recursive : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;
};

template <class Meta, class Type>
	requires is_recursive_value<Type>
struct visit_recursive<Meta, Type> : visit_delegated<visit<Meta, Type>> {
		using visit_type = visit_delegated<visit<Meta, Type>>;
		using visit_type::visit_type;
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

// Extract the target type from an `accept`-like type. This is used to know what to cast the result
// of `accept()` to.
template <class Type>
struct accept_target_of;

export template <class Type>
using accept_target_t = accept_target_of<std::remove_cvref_t<Type>>::type;

template <class Accept>
struct accept_target_of : std::type_identity<typename Accept::accept_target_type> {};

template <class Meta, class Type>
struct accept_target_of<accept<Meta, Type>> : std::type_identity<Type> {};

// Recursively invoke single-argument `accept` calls until a non-`accept`'able type is returned. `referenceable_value`'s
// are unwrapped and passed to `store`.
constexpr auto consume_accept(auto& accept, auto&& value, const auto& /*store*/)
	-> accept_target_t<decltype(accept)> {
	using value_type = accept_target_t<decltype(accept)>;
	return value_type(std::forward<decltype(value)>(value));
}

constexpr auto consume_accept(auto& accept, auto&& value, const auto& store)
	-> accept_target_t<decltype(accept)>
	requires requires { accept(std::declval<decltype(value)>()); } {
	return consume_accept(accept, accept(std::forward<decltype(value)>(value)), store);
}

template <class Type>
constexpr auto consume_accept(auto& accept, js::referenceable_value<Type> value, const auto& store)
	-> accept_target_t<decltype(accept)> {
	store(*value);
	return consume_accept(accept, *std::move(value), store);
}

template <class Type, class... Args>
constexpr auto consume_accept(auto& accept, js::deferred_receiver<Type, Args...> receiver, const auto& store)
	-> accept_target_t<decltype(accept)> {
	store(*receiver);
	std::move(receiver)();
	return consume_accept(accept, *std::move(receiver), store);
}

// TODO: It will be removed
export template <class Accept>
constexpr auto invoke_accept(Accept& accept, auto tag, const auto& visit, auto&& subject) -> accept_target_t<Accept> {
	return consume_accept(accept, accept(tag, visit, std::forward<decltype(subject)>(subject)), util::unused);
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
				accept,
				accept(tag, visit, std::forward<decltype(subject)>(subject)),
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
