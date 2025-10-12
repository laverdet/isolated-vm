module;
#include <array>
#include <span>
#include <vector>
export module isolated_js:vector.visit;
import :transfer;

namespace js {

template <class Type, size_t Size>
struct visit_subject_for<std::array<Type, Size>> : visit_subject_for<Type> {};

template <class Type>
struct visit_subject_for<std::span<Type>> : visit_subject_for<std::remove_cv_t<Type>> {};

template <class Type>
struct visit_subject_for<std::vector<Type>> : visit_subject_for<Type> {};

template <class Meta, class Type, size_t Size>
struct visit<Meta, std::array<Type, Size>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			const visit_type& visitor = *this;
			return accept(vector_n_tag<Size>{}, visitor, std::forward<decltype(value)>(value));
		}
};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit<Meta, std::remove_cv_t<Type>> {
		using visit_type = visit<Meta, std::remove_cv_t<Type>>;
		using visit_type::visit_type;

		constexpr auto operator()(auto value, auto& accept) const -> decltype(auto) {
			const visit_type& visitor = *this;
			return accept(vector_tag{}, visitor, value);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, std::span<Type>> {
		using visit_type = visit<Meta, std::span<Type>>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return util::invoke_as<visit_type>(*this, std::span{value}, accept);
		}
};

} // namespace js
