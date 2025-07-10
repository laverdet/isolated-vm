module;
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

		explicit constexpr bigint(uint64_t number) {
			resize_and_overwrite(1, [ & ](word_type* words, std::size_t /*length*/) constexpr {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				words[ 0 ] = number;
				sign_bit() = 0;
				return 1;
			});
		}

		explicit constexpr bigint(int64_t number) {
			resize_and_overwrite(1, [ & ](word_type* words, std::size_t /*length*/) constexpr {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, hicpp-signed-bitwise)
				words[ 0 ] = static_cast<word_type>(number) & ~(1LL << 63);
				// NOLINTNEXTLINE(hicpp-signed-bitwise)
				sign_bit() = static_cast<int>((number >> 63) & 1);
				return 1;
			});
		}

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
		[[nodiscard]] constexpr auto sign_bit() const -> int { return sign_bit_; }
		[[nodiscard]] constexpr auto size() const -> std::size_t { return words_.size(); }
		constexpr auto sign_bit() -> int& { return sign_bit_; }

		constexpr auto resize_and_overwrite(std::size_t size, std::invocable<word_type*, std::size_t> auto overwrite) -> void {
			words_.resize(size);
			words_.resize(overwrite(reinterpret_cast<word_type*>(words_.data()), size));
		}

	private:
		int sign_bit_ = 0;
		std::vector<util::trivial_aggregate<word_type>> words_;
};

export using number_t = std::variant<double, int32_t, uint32_t>;
export using bigint_t = std::variant<bigint, int64_t, uint64_t>;
export using string_t = std::variant<std::u16string, std::string>;

} // namespace js
