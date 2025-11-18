module;
#include <string_view>
#include <type_traits>
#include <utility>
export module isolated_js:enum_.accept;
import :enum_.types;
import :intrinsics.error;
import :transfer;
import ivm.utility;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> {
		constexpr auto operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Enum {
			auto values = accept::make_enum_map();
			auto result = values.find(util::fnv1a_hash(std::forward<decltype(subject)>(subject)));
			if (result == nullptr) {
				throw js::type_error{u"Invalid enumeration"};
			}
			return result->second;
		}

		consteval static auto make_enum_map() {
			const auto& [... values ] = enum_values<Enum>::values;
			return util::sealed_map{
				std::in_place,
				[ & ]() constexpr -> auto {
					return std::pair{util::fnv1a_hash(std::string_view{values.first}), values.second};
				}()...,
			};
		}
};

} // namespace js
