module;
#include <cassert>
export module auto_js:reference.visit;
import :intrinsics.error;
import :referential_value;
import :transfer;
import std;
import util;

namespace js {

// This takes the place of `reference_map_t` used by JS runtime visitors. References are already
// nicely laid out in a contiguous id space, so a vector is used instead of a map.
template <class Subject, class Reaccept>
struct reference_vector_of {
	private:
		using target_reference_type = Reaccept::reference_type;
		using subject_reference_type = reference_of<Subject>;
		static_assert(std::is_trivially_copyable_v<target_reference_type>);

	public:
		constexpr auto emplace_subject([[maybe_unused]] subject_reference_type reference, const auto& value) -> void {
			assert(values_storage_.size() == reference.id());
			values_storage_.emplace_back(value);
		}

		[[nodiscard]] constexpr auto lookup_or_visit(subject_reference_type subject, auto dispatch) const -> decltype(auto) {
			using target_type = decltype(dispatch());
			if (values_storage_.size() == subject.id()) {
				return dispatch();
			} else {
				// reaccept from reference
				Reaccept reaccept;
				const auto type_tag = std::type_identity<target_type>{};
				return reaccept(type_tag, values_storage_.at(subject.id()));
			}
		}

		constexpr auto clear_accepted_references() {
			values_storage_.clear();
		}

	private:
		std::vector<target_reference_type> values_storage_;
};

template <class Subject>
struct reference_vector_of<Subject, void> {
	public:
		using reference_type = reference_of<Subject>;

		[[nodiscard]] constexpr auto lookup_or_visit(reference_type subject, auto dispatch) const -> decltype(auto) {
			if (count_ == subject.id()) {
				return dispatch();
			} else {
				// TODO: It would be nice if this was a `static_assert`
				throw js::type_error{u"visited circular value in non-referential acceptor"};
			}
		}

		constexpr auto clear_accepted_references() { count_ = 0; }

	private:
		std::size_t count_ = 0;
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
			return values_storage_.lookup_or_visit(reference, [ & ]() constexpr -> accept_target_t<Accept> {
				auto insert = [ & ] -> auto {
					if constexpr (type<typename Meta::accept_reference_type> == type<void>) {
						return [](const auto& /*value*/) -> void {};
					} else {
						using reference_type = Meta::accept_reference_type::reference_type;
						return util::overloaded{
							[ &, reference ](const auto& value) -> void {
								if constexpr (requires { reference_type{value}; }) {
									values_storage_.emplace_subject(reference, value);
								}
							},
							[ &, reference ]<class Ref>(const js::referenceable_value<Ref>& value) -> void {
								values_storage_.emplace_subject(reference, *value);
							},
							[ &, reference ]<class Ref, class... Args>(const js::deferred_receiver<Ref, Args...>& receiver) -> void {
								values_storage_.emplace_subject(reference, *receiver);
							},
						};
					}
				}();
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
};

// `recursive_value_holder` visitor delegates to its underlying value visitor through a top `types()`
// instantiation.
template <class Meta, class Value, class Refs>
struct visit_recursive_value_holder;

template <class Meta, class Value>
using visit_recursive_value_holder_from = visit_recursive_value_holder<Meta, Value, typename Value::reference_types>;

template <class Meta, template <class> class Make, auto Extract>
struct visit<Meta, recursive_value_holder<Make, Extract>>
		: visit_delegated<visit_recursive_value_holder_from<Meta, recursive_value_holder<Make, Extract>>> {
	private:
		using delegated_type = visit_recursive_value_holder_from<Meta, recursive_value_holder<Make, Extract>>;
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
struct visit_recursive_value_holder<Meta, Value, util::type_pack<Refs...>>
		: visit<Meta, typename Value::value_type>,
			visit_reference_of<Meta, Refs>... {
	private:
		using value_type = Value;
		using value_of_type = value_type::value_type;
		using visit_type = visit<Meta, value_of_type>;

	public:
		explicit constexpr visit_recursive_value_holder(auto* transfer) :
				visit_type{transfer},
				visit_reference_of<Meta, Refs>{transfer}... {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, std::forward<decltype(subject)>(subject), accept);
		}

		constexpr auto reset_for_visited_value(auto&& subject) -> void {
			// Reset accepted values in `reference_of<T>` visitor
			// NOLINTNEXTLINE(bugprone-use-after-move)
			(..., visit_reference_of<Meta, Refs>::reset_for_visited_value(std::forward<decltype(subject)>(subject)));
		}

		// Infinite recursion check.
		template <class... Seen>
		consteval static auto types(util::type_pack<Seen...> recursive) {
			if constexpr ((... || (type<Seen> == type<value_type>))) {
				return util::type_pack{};
			} else {
				return visit_type::types(recursive + util::type_pack{type<value_type>});
			}
		}
};

// `referential_value` subject visitor. This is the "top" visitor, and there may be more than one
// instance of it within the `transfer` inheritance tree. When a value is visited it copies or moves
// the underlying reference storage into `visit_references_of`. It also delegates back to the
// underlying visitor.
template <class Meta, class Value, class Holder>
struct visit<Meta, referential_value<Value, Holder>> : visit<Meta, typename referential_value<Value, Holder>::value_type> {
	private:
		using visit_type = visit<Meta, typename referential_value<Value, Holder>::value_type>;
		using visit_type::reset_for_visited_value;

	public:
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			// Reset accepted values in `reference_of<T>` visitor
			reset_for_visited_value(std::forward<decltype(subject)>(subject));
			// Delegate forward to `visit_recursive_value_holder` visitor
			// NOLINTNEXTLINE(bugprone-use-after-move)
			return util::invoke_as<visit_type>(*this, *std::forward<decltype(subject)>(subject), accept);
		}
};

} // namespace js
