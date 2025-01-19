module;
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
	private:
		using first_type = visit_key_literal<Meta, property_name_v<Getter>, accept_target_t<Meta>>;
		using second_type = visit_getter<Meta, Getter>;

	public:
		constexpr explicit visit_object_member(auto visit_heritage) :
				first{visit_heritage},
				second{visit_heritage} {}

		first_type first;
		second_type second;
};

// Receives a C++ object and accepts a `struct_tag`, which is a tuple of visitors.
template <class Meta, class Type, class Getters>
struct visit_object_subject;

template <class Meta, class Type, class... Getters>
struct visit_object_subject<Meta, Type, std::tuple<Getters...>> {
	public:
		constexpr explicit visit_object_subject(auto visit_heritage) :
				visit_{visit_object_member<Meta, Getters>{visit_heritage}...} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(struct_tag<sizeof...(Getters)>{}, std::forward<decltype(value)>(value), visit_);
		}

	private:
		std::tuple<visit_object_member<Meta, Getters>...> visit_;
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
		constexpr explicit visit(auto visit_heritage) :
				visit_subject_type<Meta, Type>{visit_heritage(this)} {}
};

} // namespace js
