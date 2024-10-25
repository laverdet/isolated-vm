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

// Visitor function for C++ object types
template <class Meta, class Type, class... Getters>
struct visit<Meta, object_type<Type, std::tuple<Getters...>>> : visit<Meta, void> {
	private:
		using accept_keys_type = std::tuple<accept_key<Meta, property_name_v<Getters>>...>;
		using visit_values_type = std::tuple<visit<Meta, typename Getters::type>...>;

	public:
		constexpr visit() :
				first{accept_keys},
				second{visit_values} {}
		constexpr visit(int /*dummy*/, const auto_visit auto& /*visit*/) :
				visit{} {}

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(struct_tag<sizeof...(Getters)>{}, value, *this);
		}

	private:
		struct first_type {
				constexpr first_type(const accept_keys_type& accept_keys) :
						accept_keys{accept_keys} {}

				template <size_t Index>
				constexpr auto operator()(
					std::integral_constant<size_t, Index> /*index*/,
					// TODO: This is the subject from `Meta`
					const auto_accept auto& accept
				) const -> decltype(auto) {
					return std::get<Index>(accept_keys)(accept);
				}

				const accept_keys_type& accept_keys;
		};

		struct second_type {
			public:
				constexpr second_type(const visit_values_type& visit_values) :
						visit_values{visit_values} {}

				template <size_t Index>
				constexpr auto operator()(
					std::integral_constant<size_t, Index> /*index*/,
					const auto& dictionary,
					// TODO: This is the subject from `Meta`
					const auto_accept auto& accept
				) const -> decltype(auto) {
					using getter_type = std::tuple_element_t<Index, std::tuple<Getters...>>;
					getter_type getter{};
					return std::get<Index>(visit_values)(getter(dictionary), accept);
				}

			private:
				const visit_values_type& visit_values;
		};

		accept_keys_type accept_keys;
		visit_values_type visit_values;

	public:
		first_type first;
		second_type second;
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
