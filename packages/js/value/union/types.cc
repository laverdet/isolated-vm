module;
#include <string>
export module ivm.value:union_.types;
import :tag;
import :variant.types;

namespace ivm::js {

// Specialize to turn `std::variant` into a discriminated union
export template <class Type>
struct union_of;

// Holds typed union alternative w/ discriminant
export template <class Type>
struct alternative {
		explicit constexpr alternative(std::string discriminant) :
				discriminant{std::move(discriminant)} {}

		std::string discriminant;
};

} // namespace ivm::js
