module;
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js:struct_.visit;
import :property;
import :struct_.types;
import :transfer;
import ivm.utility;

namespace js {

// Visit a member of a struct via a getter delegate. This gets passed along to whatever acceptor
// takes the `struct_tag`.
template <class Meta, class Get>
struct visit_getter : visit<Meta, typename Get::value_type> {
		constexpr visit_getter(auto* transfer, Get get) :
				visit_type{transfer},
				get_{std::move(get)} {}

		template <class Accept>
		constexpr auto operator()(const auto& subject, const Accept& accept) -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, get_(subject), accept);
		}

	private:
		using visit_type = visit<Meta, typename Get::value_type>;
		Get get_;
};

// Visitor passed to acceptor for each member
template <class Meta, class Property>
struct visit_object_property {
	private:
		using getter_type = decltype(std::declval<Property>().get);
		using first_type = visit_key_literal<Property::name, typename Meta::accept_property_subject_type>;
		using second_type = visit_getter<Meta, getter_type>;

	public:
		constexpr visit_object_property(auto* transfer, Property property) :
				second{transfer, property.get} {}

		[[no_unique_address]] first_type first;
		[[no_unique_address]] second_type second;
};

// Helper to unpack `properties` tuple, construct individual property visitors, and forward struct
// to acceptors & getters.
template <class Meta, class Type, class Properties>
struct visit_struct_properties;

template <class Meta, class Type>
using visit_struct_properties_t = visit_struct_properties<Meta, Type, std::remove_cvref_t<decltype(struct_properties<Type>::properties)>>;

template <class Meta, class Type, class... Property>
struct visit_struct_properties<Meta, Type, js::struct_template<Property...>> {
	private:
		using properties_type = std::tuple<visit_object_property<Meta, Property>...>;

	public:
		explicit constexpr visit_struct_properties(auto* transfer) :
				properties{[ & ]() constexpr -> properties_type {
					const auto [... indices ] = util::sequence<sizeof...(Property)>;
					return {util::elide{
						util::constructor<visit_object_property<Meta, Property...[ indices ]>>,
						transfer,
						std::get<indices>(struct_properties<Type>::properties)
					}...};
				}()} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(struct_tag<sizeof...(Property)>{}, properties, std::forward<decltype(subject)>(subject));
		}

	private:
		properties_type properties;
};

// Visitor function for C++ object types
template <class Meta, transferable_struct Type>
struct visit<Meta, Type> : visit_struct_properties_t<Meta, Type> {
		using visit_struct_properties_t<Meta, Type>::visit_struct_properties_t;
};

} // namespace js
