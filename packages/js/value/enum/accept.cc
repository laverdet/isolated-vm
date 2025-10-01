module;
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
export module isolated_js:enum_.accept;
import :enum_.types;
import :transfer;
import ivm.utility;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> {
		constexpr auto operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, auto&& value) const -> Enum {
			auto values = accept::make_enum_map();
			auto result = values.find(util::djb2_hash(std::forward<decltype(value)>(value)));
			if (result == nullptr) {
				throw std::logic_error{"Invalid enumeration"};
			}
			return result->second;
		}

		consteval static auto make_enum_map() {
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) consteval {
					return util::sealed_map{
						std::in_place,
						invoke(std::integral_constant<size_t, Index>{})...
					};
				},
				[]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& value = std::get<Index>(enum_values<Enum>::values);
					return std::pair{util::djb2_hash(std::string_view{value.first}), value.second};
				},
				std::make_index_sequence<enum_values<Enum>::values.size()>{}
			);
		}
};

} // namespace js
