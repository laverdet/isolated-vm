module;
#include <array>
#include <span>
#include <vector>
export module isolated_js.vector.visit;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

template <class Type>
struct transferee_subject<std::span<Type>> : std::type_identity<Type> {};

template <class Type, size_t Size>
struct transferee_subject<std::array<Type, Size>> : std::type_identity<Type> {};

template <class Type>
struct transferee_subject<std::vector<Type>> : std::type_identity<Type> {};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit<Meta, Type> {
		constexpr explicit visit(auto_heritage auto visit_heritage, auto&&... args) :
				visit<Meta, Type>{visit_heritage(this), std::forward<decltype(args)>(args)...} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, Type>& visitor = *this;
			return accept(vector_tag{}, std::forward<decltype(value)>(value), visitor);
		}
};

template <class Meta, class Type, size_t Size>
struct visit<Meta, std::array<Type, Size>> : visit<Meta, std::span<Type>> {
		constexpr explicit visit(auto_heritage auto visit_heritage, auto&&... args) :
				visit<Meta, std::span<Type>>{visit_heritage(this), std::forward<decltype(args)>(args)...} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return visit<Meta, std::span<Type>>::operator()(std::span{std::forward<decltype(value)>(value)}, accept);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, std::span<Type>> {
		constexpr explicit visit(auto_heritage auto visit_heritage, auto&&... args) :
				visit<Meta, std::span<Type>>{visit_heritage(this), std::forward<decltype(args)>(args)...} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return visit<Meta, std::span<Type>>::operator()(std::span{std::forward<decltype(value)>(value)}, accept);
		}
};

} // namespace js
