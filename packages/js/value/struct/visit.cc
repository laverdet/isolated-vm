module;
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.visit;
import isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

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
	private:
		template <class Getter>
		using first_visit = visit<Meta, key_for<property_name_v<Getter>, accept_target_t<Meta>>>;
		template <class Getter>
		using second_visit = visit<Meta, getter_for<Getter>>;

	public:
		using visit<Meta, void>::visit;

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(struct_tag<sizeof...(Getters)>{}, std::forward<decltype(value)>(value), visit_);
		}

	private:
		std::tuple<std::pair<first_visit<Getters>, second_visit<Getters>>...> visit_;
};

// Apply `getter_delegate` to each property, filtering properties which do not have a getter.
template <class Meta, class Type, class... Properties>
struct visit<Meta, mapped_object_type<Type, std::tuple<Properties...>>>
		: visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Type, Properties>...>>>> {
		using visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Type, Properties>...>>>>::visit;
};

// Unpack object properties from `object_properties` specialization
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct visit<Meta, Type>
		: visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>> {
		using visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>::visit;
};

} // namespace js
