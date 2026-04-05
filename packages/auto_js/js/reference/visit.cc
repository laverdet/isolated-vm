module;
#include <cassert>
export module auto_js:reference.visit;
import :referential_value;
import :transfer;
import std;
import util;

namespace js {

// This takes the place of `reference_map_t` used by JS runtime visitors. References are already
// nicely laid out in a contiguous id space, so a vector is used instead of a map.
template <class Subject, class Target>
struct reference_vector_of {
	private:
		using reference_type = reference_of<Subject>;

	public:
		constexpr auto emplace_subject(reference_type reference, const auto& value) -> void {
			assert(values_storage_.size() == reference.id());
			values_storage_.emplace_back(value);
		}

		template <class Accept>
		[[nodiscard]] constexpr auto lookup_or_visit(const Accept& accept, reference_type subject, auto dispatch) const
			-> accept_target_t<Accept> {
			if (values_storage_.size() == subject.id()) {
				return dispatch();
			} else {
				// reaccept from reference
				const auto type_tag = std::type_identity<accept_target_t<Accept>>{};
				return accept(type_tag, values_storage_.at(subject.id()));
			}
		}

		constexpr auto clear_accepted_references() {
			values_storage_.clear();
		}

	private:
		std::vector<Target> values_storage_;
};

template <class Subject>
struct reference_vector_of<Subject, void> {
	public:
		using reference_type = reference_of<Subject>;

		constexpr auto emplace_subject(reference_type /*reference*/, const auto& /*value*/) -> void {}

		template <class Accept>
		[[nodiscard]] constexpr auto lookup_or_visit(const Accept& /*accept*/, reference_type /*subject*/, auto dispatch) const
			-> accept_target_t<Accept> {
			return dispatch();
		}

		constexpr auto clear_accepted_references() {}
};

// Instantiated by the top `visit<..., referential_value>` visitor (actually, instantiated by the
// `transfer_holder` instance, but the visit class requests it). The the top visitor is given a
// referential value it first sends it to the `visit_reference_of` instantiations. These visitors
// move or copy the underlying value storage. Also, this visitor handles references returned by the
// acceptor.
template <class Meta, class Type>
struct visit_reference_of : visit<Meta, Type> {
	public:
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(reference_of<Type> reference, const Accept& accept) -> accept_target_t<Accept> {
			return values_storage_.lookup_or_visit(accept, reference, [ & ]() constexpr -> accept_target_t<Accept> {
				auto insert = [ & ](const auto& value) -> void { values_storage_.emplace_subject(reference, value); };
				auto accept_ = accept_value_direct{accept, insert};
				return util::invoke_as<visit_type>(*this, std::move(references_).at(reference), accept_);
			});
		}

	protected:
		constexpr auto reset_for_visited_value(auto&& subject) -> void {
			// Copy or move reference value storage from the subject. For example: a bunch of
			// `std::string`'s or whatever.
			references_ = std::forward<decltype(subject)>(subject).references();
			// Clear out accepted references, from `accept`. For example, `v8::Local<v8::Value>`.
			values_storage_.clear_accepted_references();
		}

	private:
		reference_storage_of<Type> references_;
		reference_vector_of<Type, typename Meta::accept_reference_type> values_storage_;
};

// `reference_of` visitor delegates to `visit_reference_of`
template <class Meta, class Type>
struct visit<Meta, reference_of<Type>> : visit_delegated<visit_reference_of<Meta, Type>> {
		using visit_type = visit_delegated<visit_reference_of<Meta, Type>>;
		using visit_type::visit_type;

		consteval static auto types(auto recursive) -> auto {
			return visit<Meta, Type>::types(recursive);
		}
};

// `referential_value_of` visitor delegates to its underlying value visitor through a top `types()`
// instantiation.
template <class Meta, class Value, class Refs>
struct visit_referential_value_of;

template <class Meta, class Value>
using visit_referential_value_of_from = visit_referential_value_of<Meta, Value, typename Value::reference_types>;

template <class Meta, template <class> class Make, auto Extract>
struct visit<Meta, referential_value_of<Make, Extract>>
		: visit_delegated<visit_referential_value_of_from<Meta, referential_value_of<Make, Extract>>> {
	private:
		using delegated_type = visit_referential_value_of_from<Meta, referential_value_of<Make, Extract>>;
		using visit_type = visit_delegated<delegated_type>;

	public:
		using visit_type::visit_type;

		consteval static auto types(auto recursive) -> auto {
			return util::type_pack{type<delegated_type>} + delegated_type::types(recursive);
		}

	protected:
		constexpr auto reset_for_visited_value(auto&& subject) -> void {
			this->get().reset_for_visited_value(std::forward<decltype(subject)>(subject));
		}
};

// Lifted `types()` instantiation which pulls in the underlying value visitor (probably
// `std::variant<...>`), and also the `visit_reference_of` instances.
template <class Meta, class Value, class... Refs>
struct visit_referential_value_of<Meta, Value, util::type_pack<Refs...>>
		: visit<Meta, typename Value::value_type>,
			visit_reference_of<Meta, Refs>... {
	private:
		using value_type = Value;
		using value_of_type = value_type::value_type;
		using visit_type = visit<Meta, value_of_type>;

	public:
		explicit constexpr visit_referential_value_of(auto* transfer) :
				visit_type{transfer},
				visit_reference_of<Meta, Refs>{transfer}... {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, *std::forward<decltype(subject)>(subject), accept);
		}

		// Infinite recursion check.
		template <class... Seen>
		consteval static auto types(util::type_pack<Seen...> recursive) -> auto
			requires((type<Seen> != type<value_type>) && ...) {
			return visit_type::types(recursive + util::type_pack{type<value_type>});
		}

		consteval static auto types(auto /*recursive*/) -> auto {
			return util::type_pack{};
		}

		constexpr auto reset_for_visited_value(auto&& subject) -> void {
			// Reset accepted values in `reference_of<T>` visitor
			// NOLINTNEXTLINE(bugprone-use-after-move)
			(..., visit_reference_of<Meta, Refs>::reset_for_visited_value(std::forward<decltype(subject)>(subject)));
		}
};

// `referential_value` subject visitor. This is the "top" visitor, and there may be more than one
// instance of it within the `transfer` inheritance tree. When a value is visited it copies or moves
// the underlying reference storage into `visit_references_of`. It also delegates back to the
// underlying visitor.
template <class Meta, template <class> class Make, auto Extract>
struct visit<Meta, referential_value<Make, Extract>> : visit<Meta, typename referential_value<Make, Extract>::value_type> {
	private:
		using visit_type = visit<Meta, typename referential_value<Make, Extract>::value_type>;
		using visit_type::reset_for_visited_value;

	public:
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			// Reset accepted values in `reference_of<T>` visitor
			reset_for_visited_value(std::forward<decltype(subject)>(subject));
			// Delegate forward to `visit_referential_value_of` visitor
			// NOLINTNEXTLINE(bugprone-use-after-move)
			return util::invoke_as<visit_type>(*this, std::forward<decltype(subject)>(subject), accept);
		}
};

} // namespace js
