module;
#include <array>
#include <concepts>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
export module isolated_js.primitive.types;
import isolated_js.date;
import isolated_js.tag;
import ivm.utility;

namespace js {

export class bigint {
	public:
		using word_type = uint64_t;

		bigint() = default;

		explicit constexpr bigint(uint64_t number) :
				words_{std::from_range, std::array{number}} {}

		explicit constexpr bigint(int64_t number) :
				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				sign_bit_{static_cast<int>((number >> 63) & 1)},
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, hicpp-signed-bitwise)
				words_{std::from_range, std::array{static_cast<word_type>(number) & ~(1LL << 63)}} {}

		[[nodiscard]] constexpr auto operator==(const bigint&) const -> bool = default;

		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] constexpr operator uint64_t() const {
			if (size() == 0) {
				return 0;
			}
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			return data()[ 0 ];
		}

		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] constexpr operator int64_t() const {
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

export using number_t = std::variant<double, int32_t>;
export using bigint_t = std::variant<bigint, uint64_t>;
export using string_t = std::variant<std::u16string, std::string>;

} // namespace js
