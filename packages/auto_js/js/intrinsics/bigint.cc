module;
#include <array>
#include <concepts>
#include <cstdint>
#include <vector>
export module auto_js:intrinsics.bigint;
import util;

namespace js {

export class bigint {
	public:
		using word_type = uint64_t;

		bigint() = default;

		explicit constexpr bigint(int number) :
				bigint{int64_t{number}} {}

		explicit constexpr bigint(uint64_t number) :
				words_{std::from_range, std::array{number}} {}

		explicit constexpr bigint(int64_t number) :
				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				sign_bit_{static_cast<int>((number >> 63) & 1)},
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, hicpp-signed-bitwise)
				words_{std::from_range, std::array{static_cast<word_type>(number) & ~(1LL << 63)}} {}

		[[nodiscard]] constexpr auto operator==(const bigint&) const -> bool = default;

		[[nodiscard]] explicit constexpr operator uint64_t() const {
			if (size() == 0) {
				return 0;
			}
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			return data()[ 0 ];
		}

		[[nodiscard]] explicit constexpr operator int64_t() const {
			if (size() == 0) {
				return 0;
			}
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, hicpp-signed-bitwise)
			return static_cast<int64_t>(data()[ 0 ]) | int64_t{sign_bit()} << 63;
		}

		[[nodiscard]] constexpr auto data() const -> const word_type* { return reinterpret_cast<const word_type*>(words_.data()); }
		[[nodiscard]] constexpr auto sign_bit() -> int& { return sign_bit_; }
		[[nodiscard]] constexpr auto sign_bit() const -> int { return sign_bit_; }
		[[nodiscard]] constexpr auto size() const -> std::size_t { return words_.size(); }

		constexpr auto resize_and_overwrite(std::size_t size, std::invocable<word_type*, std::size_t> auto overwrite) -> void {
			words_.resize(size);
			words_.resize(overwrite(reinterpret_cast<word_type*>(words_.data()), size));
		}

	private:
		int sign_bit_{};
		std::vector<word_type, util::noinit_allocator<word_type>> words_;
};

} // namespace js
