module;
#include <array>
#include <span>
#include <vector>
export module auto_js:vector.visit;
import :transfer;
import :vector.vector_of;

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

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			visit_type& visitor = *this;
			return accept(vector_n_tag<Size>{}, visitor, std::forward<decltype(subject)>(subject));
		}
};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit<Meta, std::remove_cvref_t<Type>> {
		using visit_type = visit<Meta, std::remove_cvref_t<Type>>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(std::span<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			visit_type& visitor = *this;
			return accept(vector_tag{}, visitor, subject);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			visit_type& visitor = *this;
			return accept(vector_tag{}, visitor, std::forward<decltype(subject)>(subject));
		}
};

template <class Meta, class Tag, class Value>
struct visit<Meta, vector_of<Tag, Value>> : visit<Meta, Value> {
		using visit_type = visit<Meta, Value>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			visit_type& visitor = *this;
			return accept(Tag{}, visitor, std::forward<decltype(subject)>(subject));
		}
};

} // namespace js
