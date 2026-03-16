module;
#include <string_view>
#include <type_traits>
#include <utility>
export module auto_js:enum_.visit;
import :enum_.types;
import :intrinsics.error;
import :transfer;
import util;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct visit<void, Enum> {
	public:
		template <class Accept>
		constexpr auto operator()(Enum subject, const Accept& accept) const -> accept_target_t<Accept> {
			constexpr auto values = make_enum_map();
			const auto& [... entries ] = enum_values<Enum>::values;
			return util::template_switch(
				subject,
				std::tuple{util::cw<entries.second>...},
				util::overloaded{
					[ & ](auto value) {
						constexpr auto sv = values.at(values.lookup(value)).second;
						return accept(string_tag_of<char>{}, *this, sv);
					},
					[]() -> accept_target_t<Accept> { std::unreachable(); },
				}
			);
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		consteval static auto make_enum_map() {
			const auto& [... values ] = enum_values<Enum>::values;
			return util::sealed_map{
				std::in_place,
				[ & ]() constexpr -> auto {
					return std::pair{values.second, values.first};
				}()...,
			};
		}
};

} // namespace js
