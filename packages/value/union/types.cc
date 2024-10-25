module;
#include <string>
export module ivm.value:union_.types;
import :tag;
import :variant.types;

namespace ivm::value {

// Specialize to turn `std::variant` into a discriminated union
export template <class Type>
struct union_of;

// Holds typed union alternative w/ discriminant
export template <class Type>
struct alternative {
		constexpr alternative(const std::string& discriminant) :
				discriminant{discriminant} {}

		std::string discriminant;
};

} // namespace ivm::value
