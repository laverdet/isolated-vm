module;
#include <algorithm>
#include <iterator>
#include <ranges>
export module ivm.utility:ranges;

namespace util {

// Some good thoughts here. It's strange that there isn't an easier way to transform an underlying
// range.
// https://brevzin.github.io/c++/2024/05/18/range-customization/
export constexpr auto into_range(std::ranges::range auto&& range) {
	return std::forward<decltype(range)>(range);
}

export constexpr auto into_range(auto&& range)
	requires requires() {
		{ range.into_range() };
	}
{
	return std::forward<decltype(range)>(range).into_range();
}

// Subrange which expands or contracts to include or possibly exclude the given members.
export template <class Type>
class subrange_ratchet : public std::ranges::subrange<Type> {
	public:
		using std::ranges::subrange<Type>::subrange;

		auto insert(Type iterator) -> void {
			*this = {
				std::min(iterator, this->begin()),
				std::max(std::next(iterator), this->end())
			};
		}

		auto remove(Type iterator) -> void {
			auto next = std::next(iterator);
			if (this->end() == next) {
				*this = {this->begin(), iterator};
			} else if (this->begin() == iterator) {
				*this = {next, this->end()};
			}
		}
};

} // namespace util
