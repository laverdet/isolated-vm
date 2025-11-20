module;
#include <cassert>
#include <string_view>
#include <type_traits>
#include <utility>
export module auto_js:visit;
import :deferred_receiver;
import :tag;
import :transfer.types;

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
struct visit_delegated_from : std::reference_wrapper<Visit> {
		constexpr explicit visit_delegated_from(auto* transfer) :
				std::reference_wrapper<Visit>{*transfer} {}
};

// `visit` a possibly recursive subject
template <class Meta, class Type>
struct visit_maybe_recursive_type : std::type_identity<visit<Meta, Type>> {};

template <class Meta, class Type>
	requires Type::is_recursive_type
struct visit_maybe_recursive_type<Meta, Type> : std::type_identity<visit_delegated_from<visit<Meta, Type>>> {};

template <class Meta, class Type>
using visit_maybe_recursive = visit_maybe_recursive_type<Meta, Type>::type;

// Utility to pass oneself as an object entry visitor
export template <class First, class Second>
struct visit_entry_pair {
		constexpr static auto has_reference_map = js::has_reference_map(type<Second>);

		explicit constexpr visit_entry_pair(auto& self) :
				first{self},
				second{self} {}

		constexpr auto emplace_subject(const auto& subject, const auto& value) -> void {
			second.emplace_subject(subject, value);
		}

		First first;
		Second second;
};

// Unfold `Subject` through container visitors.
export template <class Subject>
struct visit_subject_for : std::type_identity<Subject> {};

// Returns the key type expected by the delegate (an instance of `visit` or `accept`) target.
export template <auto Key, class Subject>
struct visit_key_literal;

template <auto Key>
struct visit_key_literal<Key, void> {
		template <class Accept>
		constexpr auto operator()(const auto& /*could_be_literally_anything*/, const Accept& accept) -> accept_target_t<Accept> {
			using value_type = decltype(Key)::value_type;
			using character_type = std::remove_extent_t<std::remove_cvref_t<value_type>>;
			return accept(string_tag_of<character_type>{}, *this, std::basic_string_view<character_type>{Key});
		}
};

// Reference map for visitors whose acceptors don't define a `accept_reference_type`
struct null_reference_map {
		explicit null_reference_map(auto&&... /*args*/) {}

		template <class Accept>
		[[nodiscard]] constexpr auto lookup_or_visit(const Accept& /*accept*/, auto /*subject*/, auto dispatch) const -> accept_target_t<Accept> {
			return dispatch();
		}
};

// Wraps the reference map type (for example `std::unordered_map<napi_value, ...>`) with
// acceptor-aware `try_emplace`.
template <class Map>
struct reference_map_provider {
	public:
		constexpr static auto has_reference_map = true;

		explicit reference_map_provider(auto&&... args) :
				map_{std::forward<decltype(args)>(args)...} {}

		constexpr auto emplace_subject(const auto& subject, const auto& value) -> void {
			[[maybe_unused]] auto [ iterator, inserted ] = map_.try_emplace(subject, value);
			assert(inserted);
		}

		template <class Accept>
		constexpr auto lookup_or_visit(const Accept& accept, const auto& subject, auto dispatch) const -> accept_target_t<Accept> {
			using value_type = accept_target_t<Accept>;
			auto it = map_.find(subject);
			if (it == map_.end()) {
				return dispatch();
			} else {
				return (*accept)(std::type_identity<value_type>{}, it->second);
			}
		}

	private:
		Map map_;
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
