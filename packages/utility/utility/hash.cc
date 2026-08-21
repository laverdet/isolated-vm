export module util:utility.hash;
import :utility.string;
import std;

namespace util {

// Dead simple `constexpr` hash for arbitrary data.
template <class Result>
struct fnv1a_hash_of {
		static_assert(std::is_integral_v<Result>);
		static_assert(std::is_unsigned_v<Result>);

		template <class Type>
		constexpr auto operator()(std::span<const Type> data) const -> Result {
			constexpr auto xx = []() -> std::tuple<Result, Result> {
				if constexpr (sizeof(Result) == 4) {
					return {0x811c'9dc5, 0x100'0193};
				} else {
					static_assert(sizeof(Result) == 8);
					return {0xcbf2'9ce4'8422'2325, 0x100'0000'01b3};
				}
			}();
			constexpr auto offset_basis = std::get<0>(xx);
			constexpr auto prime = std::get<1>(xx);
			auto hash = offset_basis;
			for (const auto& value : data) {
				auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(Type)>>(value);
				for (auto byte : bytes) {
					hash = hash ^ byte;
					hash *= prime;
				}
			}
			return hash;
		}

		template <class Char>
		constexpr auto operator()(std::basic_string_view<Char> view) const -> Result {
			return (*this)(std::span<const Char>{view.data(), view.size()});
		}
};

export auto fnv1a_hash32 = fnv1a_hash_of<std::uint32_t>{};
export auto fnv1a_hash64 = fnv1a_hash_of<std::uint64_t>{};

// constexpr `typeid(Type).hash_code()` replacement
export template <class Result, class Type>
consteval auto type_hash_of() -> Result {
#if __clang__
	constexpr auto name = std::string_view{std::source_location::current().function_name()};
	// auto util::type_hash_of() [Result = unsigned int, Type = int]
	// auto util::type_hash_of() [Result = unsigned int, Type = (lambda at /workspace/playground/sandbox.cc:15:12)]
	if (name.contains("lambda at ")) {
		throw std::logic_error{"cannot hash a lambda type"};
	}
#elif __GNUC__
	constexpr auto name = std::string_view{__PRETTY_FUNCTION__};
	// consteval auto util::type_hash_of@util() [with Result = unsigned int; Type = int]
	// consteval auto util::type_hash_of@util() [with Result = unsigned int; Type = main()::<lambda()>]
	if (name.contains("lambda(")) {
		throw std::logic_error{"cannot hash a lambda type"};
	}
#else
#error "Unsupported compiler"
#endif
	return fnv1a_hash_of<Result>{}(name);
}

export template <class Type>
constexpr auto type_hash32 = type_hash_of<std::uint32_t, Type>();

export template <class Type>
constexpr auto type_hash64 = type_hash_of<std::uint64_t, Type>();

static_assert(type_hash32<int> != type_hash32<unsigned>);

} // namespace util
