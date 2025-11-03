module;
#include <array>
#include <bit>
#include <cstdint>
#include <source_location>
#include <string_view>
export module ivm.utility:utility.hash;

namespace util {

// `constexpr` hash for property lookup
export template <class Char>
constexpr auto fnv1a_hash(std::basic_string_view<Char> view) -> uint32_t {
	uint32_t hash = 0x811c'9dc5;
	uint32_t prime = 0x100'0193;
	for (auto character : view) {
		auto bytes = std::bit_cast<std::array<uint8_t, sizeof(Char)>>(character);
		for (auto byte : bytes) {
			hash = hash ^ byte;
			hash *= prime;
		}
	}
	return hash;
}

// constexpr `typeid(Type).hash_code()` replacement
template <class Type>
consteval auto make_type_hash() -> uint32_t {
	// "uint32_t make_type_hash() [Type = int]"
	constexpr auto name = std::source_location::current().function_name();
	return fnv1a_hash(std::string_view{name});
}

export template <class Type>
constexpr auto type_hash = make_type_hash<Type>();

} // namespace util
