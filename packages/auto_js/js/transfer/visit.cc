module;
#include <cassert>
export module auto_js:visit;
import :deferred_receiver;
import :intrinsics.error;
import :transfer.types;
import std;
import util;

namespace js {

// `Meta` type for `visit` implementations
template <class Subject, class Target, class Reference>
struct visit_meta_holder {
		using accept_property_subject_type = Target;
		using accept_reference_type = Reference;
		using visit_subject_type = Subject;
};

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		// Swallow `transfer` argument on behalf of non-meta visitors
		constexpr explicit visit(auto* /*transfer*/, auto&&... args) :
				visit<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Default `visit` selection, for `select_visit` specializations which only unfold the subject
export struct select_default_visit {
		template <class Meta, class Type>
		using visit = js::visit<Meta, Type>;
};

// Top-level visitor configuration
export template <class Subject, class... Args>
struct select_visit : select_default_visit {
		using subject_type = Subject;
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

// `visit` delegated to a related sub class passed down from the constructor
export template <class Visit>
struct visit_proxy_relative : util::reference_wrapper<Visit> {
		constexpr explicit visit_proxy_relative(auto* transfer) :
				// nb: Casting through pointer is very intentional. Since we inherit from
				// `std::reference_wrapper<T>` casting through a reference might invoke `operator T&` and
				// cause confusing compile-time or run-time errors.
				util::reference_wrapper<Visit>{*static_cast<Visit*>(transfer)} {}

		constexpr auto emplace_subject(auto reference, const auto& value) -> void {
			this->get().emplace_subject(reference, value);
		}

		consteval static auto visit_with_reference_map() -> bool { return js::visit_with_reference_map<Visit>; }
		consteval static auto types(auto recursive) -> auto { return Visit::types(recursive); }
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

		consteval static auto visit_with_reference_map() -> bool { return js::visit_with_reference_map<Second>; }

		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		First first;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		Second second;
};

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

		[[nodiscard]] constexpr auto lookup_or_visit(auto /*subject*/, auto dispatch) const -> decltype(auto) {
			return dispatch();
		}
};

// Wraps the reference map type (for example `std::unordered_map<napi_value, ...>`) with
// acceptor-aware `try_emplace`.
template <class Reference, class Map>
struct reference_map_provider {
	private:
		using map_type = Map::template type<typename Reference::reference_type>;
		using key_type = map_type::key_type;
		using mapped_type = map_type::mapped_type;
		static_assert(std::is_trivially_copyable_v<key_type>);
		static_assert(std::is_trivially_copyable_v<mapped_type>);

	public:
		constexpr auto emplace_subject(key_type subject, mapped_type value) -> void {
			// NOLINTNEXTLINE(cppcoreguidelines-slicing)
			[[maybe_unused]] auto [ iterator, inserted ] = map_.try_emplace(subject, value);
			assert(inserted);
		}

		constexpr auto lookup_or_visit(key_type subject, auto dispatch) const -> decltype(auto) {
			using target_type = decltype(dispatch());
			auto it = map_.find(subject);
			if (it == map_.end()) {
				return dispatch();
			} else {
				Reference reaccept;
				return reaccept(std::type_identity<target_type>{}, it->second);
			}
		}

		consteval static auto visit_with_reference_map() -> bool { return true; }

	private:
		map_type map_;
};

// Construct a map to store references to visited values. If the acceptor has no reference type then
// `null_reference_map` is used.
template <class Reference, class Map>
struct reference_map;

export template <class Reference, class Map>
using reference_map_t = reference_map<Reference, Map>::type;

template <class Reference, class Map>
struct reference_map : std::type_identity<reference_map_provider<Reference, Map>> {};

template <class Map>
struct reference_map<void, Map> : std::type_identity<null_reference_map> {};

// `transferList` helper which collects entries matching a projection, and removes them from the
// original list.
export template <class Subject>
auto extract_transferees(auto name_cw, auto project_transferee, auto subject_equal, std::vector<Subject>& transferees) {
	using projected_type = std::invoke_result_t<decltype(project_transferee)&, Subject&>::value_type;
	auto subjects = std::vector<projected_type>{};
	for (auto ii = transferees.begin(); ii != transferees.end();) {
		if (auto projected = project_transferee(*ii)) {
			auto is_duplicate = [ & ](const projected_type& subject) -> bool { return subject_equal(subject, *projected); };
			if (std::ranges::any_of(subjects, is_duplicate)) {
				constexpr auto name = util::make_consteval_string_view(name_cw);
				throw js::type_error{u"Transfer list contains duplicate " + std::u16string{name}};
			}
			subjects.push_back(*std::move(projected));
			*ii = transferees.back();
			transferees.pop_back();
		} else {
			++ii;
		}
	}
	return subjects;
}

// `transferList` helper which matches `subject` against the list and invokes the claim function if
// it's found, returning a std::optional of the result.
export template <class Subject>
auto claim_transferee(std::vector<Subject>& transferees, const auto& subject, auto subject_equal, auto claim)
	-> std::optional<std::invoke_result_t<decltype(claim)&, Subject>> {
	auto it = std::ranges::find_if(transferees, [ & ](const Subject& entry) -> bool { return subject_equal(entry, subject); });
	return it == transferees.end()
		? std::nullopt
		: std::optional{[ & ] -> auto {
				auto entry = std::exchange(*it, transferees.back());
				transferees.pop_back();
				return claim(entry);
			}()};
}

// Composition of visitor delegates. A subject is offered to each delegate in declaration order and
// the first one to claim it wins.
export template <class... Delegates>
class visitor_delegate_of {
	public:
		explicit visitor_delegate_of(Delegates... delegates) : delegates_{std::move(delegates)...} {}

		auto with(this auto&& self, auto operation) -> decltype(auto) {
			auto result = operation(self);
			self.finalize();
			return result;
		}

		// Finalizes each delegate which has something to finalize. A composer is itself a delegate, so
		// this is what makes one nest inside another.
		auto finalize() -> void {
			constexpr auto [... ii ] = util::sequence<sizeof...(Delegates)>;
			(..., std::get<ii>(delegates_).finalize());
		}

		template <class Visit, class Accept>
		auto operator()(auto subject, Visit& visit, const Accept& accept) -> std::optional<accept_target_t<Accept>> {
			return util::template_traverse(
				util::sequence_cw<sizeof...(Delegates)>,
				util::overloaded{
					[ & ](auto ii, auto next) -> std::optional<accept_target_t<Accept>> {
						auto& delegate = std::get<ii>(delegates_);
						if constexpr (std::invocable<decltype(delegate), decltype(subject), Visit&, const Accept&>) {
							if (auto result = delegate(subject, visit, accept)) {
								return result;
							}
						}
						return next();
					},
					[] -> std::optional<accept_target_t<Accept>> { return std::nullopt; },
				}
			);
		}

	private:
		std::tuple<Delegates...> delegates_;
};

template <class Type>
struct is_visitor_delegate : std::bool_constant<false> {};

template <class... Delegates>
struct is_visitor_delegate<visitor_delegate_of<Delegates...>> : std::bool_constant<true> {};

export template <class Type>
concept visitor_delegate_type = is_visitor_delegate<Type>::value;

// A internal transferred value claimed by transfer list delegate.
export template <class Subject, class Type>
class transferred_value {
	public:
		explicit transferred_value(Subject subject, const Type& value) :
				subject_{std::move(subject)},
				value_{value} {}
		explicit transferred_value(Subject subject, Type&& value) :
				subject_{std::move(subject)},
				value_{std::move(value)} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Subject() const { return subject_; }
		auto operator*() && -> Type { return std::move(value_); }

	private:
		Subject subject_;
		Type value_;
};

} // namespace js
