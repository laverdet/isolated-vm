module;
#include <cstdint>
#include <stdexcept>
#include <type_traits>
export module ivm.value:enum_.accept;
import ivm.utility;
import :enum_.types;
import :tag;
import :visit;

namespace ivm::value {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> : accept<void, void> {
		using hash_type = uint32_t;
		using accept<void, void>::accept;

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
