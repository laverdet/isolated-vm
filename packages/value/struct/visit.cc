module;
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.value:struct_.visit;
import :struct_.helpers;
import :tag;
import :visit;

namespace ivm::value {

template <class Getter>
struct getter_for;

// Visit a member of a struct via a getter delegate
template <class Meta, class Getter>
struct visit<Meta, getter_for<Getter>> : visit<Meta, void> {
	public:
		using visit<Meta, void>::visit;

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			return visit_(getter(value), accept);
		}

	private:
		Getter getter{};
		visit<Meta, typename Getter::type> visit_;
};

// Visitor function for C++ object types
template <class Meta, class Type, class... Getters>
struct visit<Meta, object_type<Type, std::tuple<Getters...>>> : visit<Meta, void> {
	public:
		using visit<Meta, void>::visit;

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(struct_tag<sizeof...(Getters)>{}, value, *this);
		}

	public:
		visit<Meta, std::tuple<key_for<property_name_v<Getters>, accept_target_t<Meta>>...>> first;
		visit<Meta, std::tuple<getter_for<Getters>...>> second;
};

// Apply `getter_delegate` to each property, filtering properties which do not have a getter.
template <class Meta, class Type, class... Properties>
struct visit<Meta, mapped_object_type<Type, std::tuple<Properties...>>>
		: visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Properties>...>>>> {
		using visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Properties>...>>>>::visit;
};

// Unpack object properties from `object_properties` specialization
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct visit<Meta, Type>
		: visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>> {
		using visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>::visit;
};

} // namespace ivm::value
