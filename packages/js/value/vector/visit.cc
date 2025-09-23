module;
#include <array>
#include <span>
#include <vector>
export module isolated_js.vector.visit;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

template <class Type, size_t Size>
struct transferee_subject<std::array<Type, Size>> : std::type_identity<Type> {};

template <class Type>
struct transferee_subject<std::span<Type>> : std::type_identity<std::remove_cv_t<Type>> {};

template <class Type>
struct transferee_subject<std::vector<Type>> : std::type_identity<Type> {};

template <class Meta, class Type, size_t Size>
struct visit<Meta, std::array<Type, Size>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			const visit_type& visitor = *this;
			return accept(vector_n_tag<Size>{}, std::forward<decltype(value)>(value), visitor);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit<Meta, std::remove_cv_t<Type>> {
		using visit_type = visit<Meta, std::remove_cv_t<Type>>;
		using visit_type::visit_type;

		constexpr auto operator()(auto value, const auto& accept) const -> decltype(auto) {
			const visit_type& visitor = *this;
			return accept(vector_tag{}, value, visitor);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, std::span<Type>> {
		using visit_type = visit<Meta, std::span<Type>>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return visit_type::operator()(std::span{value}, accept);
		}
};

} // namespace js
