module;
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.visit;
import isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Visit a member of a struct via a getter delegate
template <class Meta, class Getter>
struct visit_getter : visit<Meta, typename Getter::type> {
	public:
		using visit<Meta, typename Getter::type>::visit;

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, typename Getter::type>& visitor = *this;
			return visitor(getter(value), accept);
		}

	private:
		Getter getter{};
};

// Visitor passed to acceptor for each member
template <class Meta, class Getter>
struct visit_object_member {
	public:
		using first_type = visit_key_literal<property_name_v<Getter>, accept_target_t<Meta>>;
		using second_type = visit_getter<Meta, Getter>;

		constexpr explicit visit_object_member(auto_heritage auto visit_heritage) :
				second{visit_heritage} {}

		first_type first;
		second_type second;
};

// This takes the place of a simple `std::pair`. Building a tuple of pairs was causing the peculiar
// error: "substitution into constraint expression resulted in a non-constant expression" with a
// useless trace. Breaking it out into this simpler class fixes the issue.
// Maybe related to this: https://github.com/llvm/llvm-project/issues/93327
template <class Meta, class Getter>
struct resolved_visit_object_member {
		using first_type = visit_key_literal<property_name_v<Getter>, accept_target_t<Meta>>;
		using second_type = visit_getter<Meta, Getter>;

		constexpr resolved_visit_object_member(const visit_object_member<Meta, Getter>& member, const auto& accept_or_visit) :
				first{member.first, accept_or_visit},
				second{member.second} {}

		first_type::visit first;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		const second_type& second;
};

// Receives a C++ object and accepts a `struct_tag`, which is a tuple of visitors.
template <class Meta, class Type, class Getters>
struct visit_object_subject;

template <class Meta, class Type, class... Getters>
struct visit_object_subject<Meta, Type, std::tuple<Getters...>> {
	public:
		constexpr explicit visit_object_subject(auto_heritage auto visit_heritage) :
				object_members_{util::make_tuple_in_place(
					[ & ] constexpr { return visit_object_member<Meta, Getters>{visit_heritage}; }...
				)} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			// For `struct_tag` we need to make a tuple of visitor pairs. The first element is a
			// `visit_key_literal::visit` instance passed by value. It's initialized with the underlying
			// `visit_key_literal` instance and also the `accept` instance, which hopefully
			// `visit_key_literal::visit` will know what to do with. The second element is simply a
			// reference to the existing visitor in `visit_object_member`.
			auto make_visit = [ & ]() {
				return std::invoke(
					[ & ]<std::size_t... Indices>(const auto& invoke, std::index_sequence<Indices...> /*indices*/) constexpr {
						return std::tuple{invoke(std::integral_constant<size_t, Indices>{})...};
					},
					[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
						using getter_type = util::select_t<Index, Getters...>;
						const auto& member = std::get<Index>(object_members_);
						return resolved_visit_object_member<Meta, getter_type>{member, accept};
					},
					std::make_index_sequence<sizeof...(Getters)>{}
				);
			};
			return accept(struct_tag<sizeof...(Getters)>{}, std::forward<decltype(value)>(value), make_visit());
		}

	private:
		std::tuple<visit_object_member<Meta, Getters>...> object_members_;
};

// Helper to unpack `properties` tuple, apply getter delegate, filter void members, and repack for
// `visit_object_subject`
template <class Type, class Properties>
struct expand_object_getters;

template <class Type, class Properties>
using expand_object_getters_t = expand_object_getters<Type, Properties>::type;

template <class Type, class... Properties>
struct expand_object_getters<Type, std::tuple<Properties...>>
		: std::type_identity<filter_void_members<getter_delegate<Type, Properties>...>> {};

template <class Meta, class Type>
using visit_subject_type =
	visit_object_subject<Meta, Type, expand_object_getters_t<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>;

// Visitor function for C++ object types
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct visit<Meta, Type> : visit_subject_type<Meta, Type> {
		constexpr explicit visit(auto_heritage auto visit_heritage) :
				visit_subject_type<Meta, Type>{visit_heritage(this)} {}
};

} // namespace js
