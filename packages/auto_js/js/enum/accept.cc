export module auto_js:enum_.accept;
import :transfer;
import std;
import util;

namespace js {

// Matches an enumerated string to an underlying enum value.
template <class Enum>
	requires std::is_enum_v<Enum>
struct accept<void, Enum> {
	public:
		constexpr auto operator()(string_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Enum {
			constexpr auto values = make_enum_map();
			auto find = [ & ](auto view) -> auto* { return values.find(util::fnv1a_hash32(view)); };
			auto result = [ & ]() -> auto* {
				if constexpr (requires { subject.view_string(find); }) {
					return subject.view_string(find);
				} else {
					return find(std::u16string_view{std::u16string{std::forward<decltype(subject)>(subject)}});
				}
			}();
			if (result == nullptr) {
				throw js::type_error{u"Invalid enumeration"};
			}
			return result->second;
		}

		consteval static auto accept_tags_of() { return std::tuple{string_tag{}}; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		consteval static auto make_enum_map() {
			constexpr auto& [... values ] = enum_values<Enum>::values;
			return util::sealed_map{
				std::in_place,
				std::pair{util::fnv1a_hash32(std::u16string_view{values.first}), values.second}...,
			};
		}
};

} // namespace js
