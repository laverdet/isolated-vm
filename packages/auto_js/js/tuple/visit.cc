export module auto_js:tuple.visit;
import :transfer;
import std;
import util;

namespace js {

template <class Meta, std::size_t Offset, class... Types>
struct visit_tuple_of {
	private:
		using visitors_type = std::tuple<visit<Meta, std::remove_cvref_t<Types>>...>;

	public:
		constexpr explicit visit_tuple_of(auto* transfer) :
				visit_{[ = ]() constexpr -> visitors_type {
					const auto [... indices ] = util::sequence<sizeof...(Types)>;
					return {util::elide(util::constructor<std::tuple_element_t<indices, visitors_type>>, transfer)...};
				}()} {}

		template <std::size_t Index, class Accept>
		constexpr auto operator()(std::integral_constant<std::size_t, Index> /*index*/, auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return std::get<Index>(visit_)(std::get<Index + Offset>(std::forward<decltype(subject)>(subject)), accept);
		}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(tuple_tag<sizeof...(Types)>{}, *this, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto recursive) -> auto {
			const auto [... types ] = util::make_type_pack(type<visitors_type>);
			return util::pack_concat(type_t<types>::types(recursive)...);
		}

	private:
		visitors_type visit_;
};

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> : visit_tuple_of<Meta, 0, Types...> {
		using visit_tuple_of<Meta, 0, Types...>::visit_tuple_of;
};

// You can use `std::tuple{std::in_place, ...}` to avoid the stupid `std::pair` constructor that
// always gets me.
template <class Meta, class... Types>
struct visit<Meta, std::tuple<std::in_place_t, Types...>> : visit_tuple_of<Meta, 1, Types...> {
		using visit_tuple_of<Meta, 1, Types...>::visit_tuple_of;
};

} // namespace js
