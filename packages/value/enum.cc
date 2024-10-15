module;
#include <cstdint>
#include <stdexcept>
#include <type_traits>
export module ivm.value:enumeration;
import ivm.utility;
import :tag;
import :visit;

namespace ivm::value {

// You override this with pairs of your enum values
export template <class Enum>
struct enum_values;

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> {
		using hash_type = uint32_t;

		template <class String>
		constexpr auto operator()(string_tag_of<String> /*tag*/, auto&& value) const -> Enum {
			auto values = accept::enum_map();
			auto result = values.get(String{std::forward<decltype(value)>(value)});
			if (result == nullptr) {
				throw std::logic_error("Invalid enumeration");
			}
			return *result;
		}

		consteval static auto enum_map() {
			auto values = enum_values<Enum>::values;
			return util::prehashed_string_map<Enum, values.size()>{values};
		}
};

} // namespace ivm::value
