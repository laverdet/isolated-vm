module;
#include "auto_js/no_unique_address.h"
#include <tuple>
#include <type_traits>
#include <utility>
export module auto_js:struct_.visit;
import :property;
import :struct_.types;
import :transfer;
import util;

namespace js {

// Visit a member of a struct via a getter delegate. This gets passed along to whatever acceptor
// takes the `struct_tag`.
template <class Meta, class Get>
struct visit_getter : visit<Meta, typename Get::value_type> {
		constexpr visit_getter(auto* transfer, Get get) :
				visit_type{transfer},
				get_{std::move(get)} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, get_(std::forward<decltype(subject)>(subject)), accept);
		}

	private:
		using visit_type = visit<Meta, typename Get::value_type>;
		Get get_;
};

// Visitor simulating `visit<M, std::pair<T, U>>`
// TODO: Lift key storage up to top `transfer_holder`
template <class Meta, class Property>
struct visit_object_property {
	private:
		using getter_type = decltype(std::declval<Property>().get);
		using first_type = visit_key_literal<Property::name, typename Meta::accept_property_subject_type>;
		using second_type = visit_getter<Meta, getter_type>;

	public:
		constexpr visit_object_property(auto* transfer, Property property) :
				first{transfer},
				second{transfer, property.get} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return {
				first(std::type_identity<void>{}, accept.first),
				second(std::forward<decltype(subject)>(subject), accept.second),
			};
		}

		consteval static auto types(auto recursive) -> auto {
			return visit<Meta, typename getter_type::value_type>::types(recursive);
		}

		NO_UNIQUE_ADDRESS first_type first;
		NO_UNIQUE_ADDRESS second_type second;
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
		using descriptor_type = struct_properties<Type>;
		using properties_type = std::tuple<visit_object_property<Meta, Property>...>;

	public:
		explicit constexpr visit_struct_properties(auto* transfer) :
				properties{[ = ]() constexpr -> properties_type {
					const auto [... indices ] = util::sequence<sizeof...(Property)>;
					return {util::elide{
						util::constructor<visit_object_property<Meta, Property...[ indices ]>>,
						transfer,
						std::get<indices>(descriptor_type::properties)
					}...};
				}()} {}

		template <std::size_t Index, class Accept>
		constexpr auto operator()(std::integral_constant<std::size_t, Index> /*index*/, auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return std::get<Index>(properties)(std::forward<decltype(subject)>(subject), accept);
		}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(struct_tag<sizeof...(Property)>{}, *this, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto recursive) -> auto {
			const auto [... properties ] = descriptor_type::properties.as_tuple();
			return util::pack_concat(visit_object_property<Meta, decltype(properties)>::types(recursive)...);
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
