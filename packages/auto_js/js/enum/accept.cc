module;
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
export module auto_js:enum_.accept;
import :enum_.types;
import :intrinsics.error;
import :transfer;
import util;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> {
	public:
		constexpr auto operator()(string_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Enum {
			constexpr auto values = make_enum_map();
			auto string_value = std::u16string{std::forward<decltype(subject)>(subject)};
			auto result = values.find(util::fnv1a_hash(std::u16string_view{string_value}));
			if (result == nullptr) {
				throw js::type_error{u"Invalid enumeration"};
			}
			return result->second;
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		consteval static auto make_enum_map() {
			const auto& [... values ] = enum_values<Enum>::values;
			return util::sealed_map{
				std::in_place,
				[ & ]() constexpr -> auto {
					// NOLINTNEXTLINE(modernize-type-traits)
					return std::pair{util::fnv1a_hash(std::u16string_view{values.first}), values.second};
				}()...,
			};
		}
};

} // namespace js
