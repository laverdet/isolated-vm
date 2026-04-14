module;
#include <cassert>
export module auto_js:visit;
import :deferred_receiver;
import :tag;
import :transfer.types;
import std;

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

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// `visit` delegated to a sub class passed down from the constructor
export template <class Visit>
struct visit_delegated : util::reference_wrapper<Visit> {
		constexpr explicit visit_delegated(auto* transfer) :
				// nb: Casting through pointer is very intentional. Since we inherit from
				// `std::reference_wrapper<T>` casting through a reference might invoke `operator T&` and
				// cause confusing compile-time or run-time errors.
				util::reference_wrapper<Visit>{*static_cast<Visit*>(transfer)} {}

		constexpr auto emplace_subject(auto reference, const auto& value) -> void {
			this->get().emplace_subject(reference, value);
		}

		consteval static auto has_reference_map() -> bool { return js::has_reference_map(type<Visit>); }
};

// Utility to pass oneself as an object entry visitor
export template <class First, class Second>
struct visit_entry_pair {
		explicit constexpr visit_entry_pair(auto& self) :
				first{self},
				second{self} {}

		constexpr auto emplace_subject(const auto& subject, const auto& value) -> void {
			second.emplace_subject(subject, value);
		}

		consteval static auto has_reference_map() -> bool { return js::has_reference_map(type<Second>); }

		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		First first;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
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
		explicit constexpr visit_key_literal(auto* /*transfer*/) {}

		template <class Accept>
		constexpr auto operator()(const auto& /*could_be_literally_anything*/, const Accept& accept) -> accept_target_t<Accept> {
			auto key_view = util::make_consteval_string_view(Key);
			using character_type = decltype(key_view)::value_type;
			return accept(string_tag_of<character_type>{}, *this, key_view);
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
	private:
		using key_type = Map::key_type;
		using mapped_type = Map::mapped_type;
		static_assert(std::is_trivially_copyable_v<key_type>);
		static_assert(std::is_trivially_copyable_v<mapped_type>);

	public:
		explicit reference_map_provider(auto&&... args) :
				map_{std::forward<decltype(args)>(args)...} {}
		constexpr auto emplace_subject(key_type subject, mapped_type value) -> void {
			// NOLINTNEXTLINE(cppcoreguidelines-slicing)
			[[maybe_unused]] auto [ iterator, inserted ] = map_.try_emplace(subject, value);
			assert(inserted);
		}

		template <class Accept>
		constexpr auto lookup_or_visit(const Accept& accept, key_type subject, auto dispatch) const -> accept_target_t<Accept> {
			using value_type = accept_target_t<Accept>;
			auto it = map_.find(subject);
			if (it == map_.end()) {
				return dispatch();
			} else {
				return accept(std::type_identity<value_type>{}, it->second);
			}
		}

		consteval static auto has_reference_map() -> bool { return true; }

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
