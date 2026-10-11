export module auto_js:vector.visit;
import :transfer;
import :vector.vector_of;
import std;

namespace js {

// `std::array`
template <std::size_t Size, class Visit>
struct visit_array : Visit {
		using Visit::Visit;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			Visit& visitor = *this;
			return accept(vector_n_tag<Size>{}, visitor, std::forward<decltype(subject)>(subject));
		}
};

// `std::span`
template <class Type, class Visit>
struct visit_span : Visit {
		using Visit::Visit;

		template <class Accept>
		constexpr auto operator()(std::span<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			Visit& visitor = *this;
			return accept(vector_tag{}, visitor, subject);
		}
};

// Ranges, `std::vector` for example
template <class Type, class Visit>
struct visit_range : Visit {
		using Visit::Visit;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			using tag_type = tagged_range<Type>::type;
			Visit& visitor = *this;
			// TODO: I'm not sure this invocation to 'dispatch_referenceable' is cromulent
			return dispatch_referenceable(accept(tag_type{}, visitor, std::forward<decltype(subject)>(subject)));
		}
};

// `js::select_visit` helper for each container
template <class Element, class... Args>
struct select_visit_container {
	private:
		using select_type = select_visit<Element, Args...>;

	public:
		using subject_type = select_type::subject_type;
		template <class Meta>
		using element_visit = select_type::template visit<Meta, Element>;
};

// `js::select_visit` configurations
template <class Type, std::size_t Size, class... Args>
struct select_visit<std::array<Type, Size>, Args...> : select_visit_container<Type, Args...> {
		template <class Meta, class Subject>
		using visit = visit_array<Size, typename select_visit_container<Type, Args...>::template element_visit<Meta>>;
};

template <class Type, class... Args>
struct select_visit<std::span<Type>, Args...> : select_visit_container<std::remove_cv_t<Type>, Args...> {
		template <class Meta, class Subject>
		using visit = visit_span<Type, typename select_visit_container<std::remove_cv_t<Type>, Args...>::template element_visit<Meta>>;
};

template <transferable_range Type, class... Args>
struct select_visit<Type, Args...> : select_visit_container<std::ranges::range_value_t<Type>, Args...> {
		template <class Meta, class Subject>
		using visit = visit_range<Type, typename select_visit_container<std::ranges::range_value_t<Type>, Args...>::template element_visit<Meta>>;
};

// `visit` instantiations delegating to anonymous implementations above
template <class Meta, class Type, std::size_t Size>
struct visit<Meta, std::array<Type, Size>> : visit_array<Size, visit<Meta, Type>> {
		using visit_array<Size, visit<Meta, Type>>::visit_array;
};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit_span<Type, visit<Meta, std::remove_cvref_t<Type>>> {
		using visit_span<Type, visit<Meta, std::remove_cvref_t<Type>>>::visit_span;
};

template <class Meta, transferable_range Type>
struct visit<Meta, Type> : visit_range<Type, visit<Meta, std::ranges::range_value_t<Type>>> {
		using visit_range<Type, visit<Meta, std::ranges::range_value_t<Type>>>::visit_range;
};

// `js::dictionary` and related classes
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
