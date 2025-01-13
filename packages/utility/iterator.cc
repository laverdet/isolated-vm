module;
#include <iterator>
#include <ranges>
export module ivm.utility:iterator;

namespace util {

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
			if (this->begin() == iterator) {
				*this = {
					std::next(iterator),
					this->end()
				};
			} else if (this->end() == std::next(iterator)) {
				*this = {
					this->begin(),
					iterator
				};
			}
		}
};

} // namespace util
