module;
#include <stdexcept>
#include <string>
#include <type_traits>
export module isolated_js.enum_.accept;
import isolated_js.enum_.types;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> : accept<void, void> {
		using accept<void, void>::accept;

		constexpr auto operator()(string_tag_of<char> /*tag*/, auto&& value) const -> Enum {
			auto values = accept::enum_map();
			auto result = values.get(std::string{std::forward<decltype(value)>(value)});
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

} // namespace js
