module;
#include <cassert>
#include <utility>
#include <vector>
export module isolated_js:reference.visit;
import :referential_value;
import :transfer;
import ivm.utility;

namespace js {

// This takes the place of `reference_map_t` used by JS runtime visitors. References are already
// nicely laid out in a contiguous id space, so a vector is used instead of a map.
template <class Subject, class Target>
struct reference_vector_of {
	private:
		using reference_type = reference_of<Subject>;

	public:
		template <class Accept>
		constexpr auto lookup_or_visit(Accept& accept, reference_type subject, auto dispatch) const
			-> accept_target_t<Accept> {
			if (values_storage_.size() == subject.id()) {
				return dispatch();
			} else {
				using value_type = accept_target_t<Accept>;
				return consume_accept(
					*accept,
					(*accept)(std::type_identity<value_type>{}, std::move(values_storage_).at(subject.id())),
					util::unused
				);
			}
		}

		template <class Accept>
		constexpr auto try_emplace(Accept& accept, auto tag, const auto& visit, reference_type reference, auto&& subject)
			-> accept_target_t<Accept> {
			using accept_direct_type = std::remove_cvref_t<decltype(*accept)>;
			return consume_accept(
				*accept,
				util::invoke_as<accept_direct_type>(accept, tag, visit, std::forward<decltype(subject)>(subject)),
				[ & ](auto&& value) -> void {
					assert(values_storage_.size() == reference.id());
					values_storage_.emplace_back(std::forward<decltype(value)>(value));
				}
			);
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

		template <class Accept>
		constexpr auto lookup_or_visit(Accept& /*accept*/, reference_type /*subject*/, auto dispatch) const
			-> accept_target_t<Accept> {
			return dispatch();
		}

		template <class Accept>
		constexpr auto try_emplace(Accept& accept, auto tag, const auto& visit, reference_type /*reference*/, auto&& subject) const
			-> accept_target_t<Accept> {
			return consume_accept(
				accept,
				accept(tag, visit, std::forward<decltype(subject)>(subject)),
				util::unused
			);
		}

		constexpr auto clear_accepted_references() const {}
};

// Instantiated by the top `visit<..., referential_value>` visitor. The the top visitor is given a
// referential value it first sends it to the `visit_reference_of` instantiations. These visitors
// move or copy the underlying value storage. Also, this visitor handles references returned by the
// acceptor.
template <class Meta, class Type>
struct visit_reference_of : visit<Meta, Type> {
	public:
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(reference_of<Type> reference, Accept& accept) const -> accept_target_t<Accept> {
			using target_type = accept_target_t<Accept>;
			return values_storage_.lookup_or_visit(accept, reference, [ & ]() constexpr -> target_type {
				auto accept_ = accept_with_callback{
					std::type_identity<target_type>{},
					[ & ](auto tag, auto&& subject) -> target_type {
						return values_storage_.try_emplace(accept, tag, *this, reference, std::forward<decltype(subject)>(subject));
					}
				};
				return util::invoke_as<visit_type>(*this, std::move(references_).at(reference), accept_);
			});
		}

	protected:
		constexpr auto reset_for_visited_value(auto&& subject) const -> void {
			// Copy or move reference value storage from the subject. For example: a bunch of
			// `std::string`'s or whatever.
			references_ = std::forward<decltype(subject)>(subject).references();
			// Clear out accepted references, from `accept`. For example, `v8::Local<v8::Value>`.
			values_storage_.clear_accepted_references();
		}

	private:
		mutable reference_storage_of<Type> references_;
		mutable reference_vector_of<Type, typename Meta::accept_reference_type> values_storage_;
};

// `reference_of` visitor delegates to `visit_reference_of`
template <class Meta, class Type>
struct visit<Meta, reference_of<Type>> : visit_delegated_from<visit_reference_of<Meta, Type>> {
		using visit_type = visit_delegated_from<visit_reference_of<Meta, Type>>;
		using visit_type::visit_type;
};

// `recursive_refs_value` visitor delegates to its underlying value visitor. It also instantiates
// `visit_reference_of` instances for each reference type.
template <class Meta, class Value, class Refs>
struct visit_recursive_refs_value;

template <class Meta, class Value>
using visit_recursive_refs_value_with = visit_recursive_refs_value<Meta, Value, typename Value::reference_types>;

template <class Meta, template <class> class Make, auto Extract>
struct visit<Meta, recursive_refs_value<Make, Extract>> : visit_recursive_refs_value_with<Meta, recursive_refs_value<Make, Extract>> {
		using visit_type = visit_recursive_refs_value_with<Meta, recursive_refs_value<Make, Extract>>;
		using visit_type::visit_type;
};

template <class Meta, class Value, class... Refs>
struct visit_recursive_refs_value<Meta, Value, util::type_pack<Refs...>>
		: visit<Meta, typename Value::value_type>,
			visit_reference_of<Meta, Refs>... {
	public:
		using visit_type = visit<Meta, typename Value::value_type>;
		explicit constexpr visit_recursive_refs_value(auto* transfer) :
				visit_type{transfer},
				visit_reference_of<Meta, Refs>{transfer}... {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, Accept& accept) const -> accept_target_t<Accept> {
			// nb: `invoke_as<visit_type>` cannot be used here since this is the recursive linchpin in the
			// whole thing
			return visit_type::operator()(*std::forward<decltype(subject)>(subject), accept);
		}

	protected:
		constexpr auto reset_for_visited_value(auto&& subject) const -> void {
			// Reset accepted values in `reference_of<T>` visitor
			(..., visit_reference_of<Meta, Refs>::reset_for_visited_value(std::forward<decltype(subject)>(subject)));
		}
};

// `referential_value` subject visitor. When a value is visited it copies or moves the underlying
// reference storage into `visit_references_of`. It also delegates back to the underlying visitor.
template <class Meta, template <class> class Make, auto Extract>
struct visit<Meta, referential_value<Make, Extract>> : visit<Meta, typename referential_value<Make, Extract>::value_type> {
		using visit_type = visit<Meta, typename referential_value<Make, Extract>::value_type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, Accept& accept) const -> accept_target_t<Accept> {
			// Reset accepted values in `reference_of<T>` visitor
			this->reset_for_visited_value(std::forward<decltype(subject)>(subject));
			// Delegate forward to `visit_recursive_refs_value` visitor
			return util::invoke_as<visit_type>(*this, std::forward<decltype(subject)>(subject), accept);
		}
};

} // namespace js
