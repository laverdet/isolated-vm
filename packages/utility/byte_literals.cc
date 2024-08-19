module;
#include <cstddef>
export module ivm.utility:byte_literals;

export namespace byte_literals {

constexpr auto operator""_kb(unsigned long long megabytes) -> size_t {
	return megabytes << 10U;
}

constexpr auto operator""_mb(unsigned long long megabytes) -> size_t {
	return megabytes << 20U;
}

constexpr auto operator""_gb(unsigned long long gigabytes) -> size_t {
	return gigabytes << 20U;
}

} // namespace byte_literals
