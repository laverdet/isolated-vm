export module util:utility.string;
import :utility.constant_wrapper;
import std;

namespace util {

// Helper to make `std::string_view` from a constant_wrapper or string literal
export template <class Char, std::size_t Size>
struct consteval_string_view : public std::basic_string_view<Char> {
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		constexpr explicit consteval_string_view(const Char (&string)[ Size + 1 ]) noexcept : std::basic_string_view<Char>{string, Size} {}
};

template <class Char, std::size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
consteval_string_view(const Char (&string)[ Size ]) -> consteval_string_view<Char, Size - 1>;

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
template <class Char, std::size_t Extent, fixed_value<Char[ Extent ]> Value>
consteval_string_view(util::constant_wrapper<Value>) -> consteval_string_view<Char, Extent - 1>;

// Helper for `codepoint_char_sequence` which stores an array of the most characters it will take to
// represent a codepoint in a given character type.
template <class Char, std::size_t Extent>
class codepoint_char_container {
	public:
		using value_type = Char;
		using container_type = std::array<value_type, Extent>;
		using iterator = container_type::const_iterator;

		// NOLINTNEXTLINE(modernize-use-equals-default)
		constexpr codepoint_char_container() {};

		[[nodiscard]] constexpr auto begin() const -> iterator { return chars_.begin(); }
		[[nodiscard]] constexpr auto data() -> container_type& { return chars_; }
		[[nodiscard]] constexpr auto end() const -> iterator { return std::ranges::find(chars_, char32_t{0}); }
		[[nodiscard]] constexpr auto size() const -> std::size_t { return end() - begin(); }

	private:
		container_type chars_;
};

// Range of characters encoding a single Unicode codepoint
template <class Char>
class codepoint_char_range;

// Latin-1, or "one-byte".
template <>
class codepoint_char_range<char> : public codepoint_char_container<char, 1> {
	public:
		explicit constexpr codepoint_char_range(char32_t codepoint) {
			if (codepoint > std::numeric_limits<char>::max()) {
				throw std::range_error{"Codepoint out of range for ASCII, 'char'"};
			}
			data()[ 0 ] = static_cast<char>(codepoint);
		}
};

// UTF-8, up to 4 bytes (both char and char8_t)
template <class Char>
class codepoint_utf8_range : public codepoint_char_container<Char, 4> {
	public:
		using codepoint_char_container<Char, 4>::data;
		constexpr static char32_t max = std::numeric_limits<char32_t>::max();

		explicit constexpr codepoint_utf8_range(char32_t codepoint) {
			constexpr auto to = [](unsigned segment) -> Char {
				return std::bit_cast<Char>(static_cast<Char>(segment));
			};
			if (codepoint < 0x80) {
				data()[ 0 ] = to(codepoint);
				data()[ 1 ] = 0;
			} else if (codepoint < 0x800) {
				data()[ 0 ] = to((codepoint >> 6) + 0xc0);
				data()[ 1 ] = to((codepoint & 0x3f) + 0x80);
				data()[ 2 ] = 0;
			} else if (codepoint < 0x1'0000) {
				data()[ 0 ] = to((codepoint >> 12) + 0xe0);
				data()[ 1 ] = to(((codepoint >> 6) & 0x3f) + 0x80);
				data()[ 2 ] = to((codepoint & 0x3f) + 0x80);
				data()[ 3 ] = 0;
			} else if (codepoint < 0x11'0000) {
				data()[ 0 ] = to((codepoint >> 18) + 0xf0);
				data()[ 1 ] = to(((codepoint >> 12) & 0x3f) + 0x80);
				data()[ 2 ] = to(((codepoint >> 6) & 0x3f) + 0x80);
				data()[ 3 ] = to((codepoint & 0x3f) + 0x80);
			} else {
				std::unreachable();
			}
		}
};

template <>
class codepoint_char_range<char8_t> : public codepoint_utf8_range<char8_t> {
		using codepoint_utf8_range<char8_t>::codepoint_utf8_range;
};

// UTF-16, up to 2 `char16_t`'s
template <>
class codepoint_char_range<char16_t> : public codepoint_char_container<char16_t, 2> {
	public:
		constexpr static char32_t max = std::numeric_limits<char32_t>::max();

		explicit constexpr codepoint_char_range(char32_t codepoint) {
			if (codepoint < 0x1'0000) {
				data()[ 0 ] = static_cast<char16_t>(codepoint);
				data()[ 1 ] = 0;
			} else if (codepoint < 0x11'0000) {
				auto twenty_bits = codepoint - 0x1'0000;
				data()[ 0 ] = static_cast<char16_t>((twenty_bits / 0x400) + 0xd800);
				data()[ 1 ] = static_cast<char16_t>((twenty_bits % 0x400) + 0xdc00);
			} else {
				std::unreachable();
			}
		}
};

// UTF-32, easy.
template <>
class codepoint_char_range<char32_t> : public codepoint_char_container<char32_t, 1> {
	public:
		explicit constexpr codepoint_char_range(char32_t codepoint) { data()[ 0 ] = codepoint; }
};

// Codepoint interpolation forward reader, converting a range of characters into a range of
// `char32_t`.
template <class Char>
class codepoint_forward_view;

template <class Char>
codepoint_forward_view(std::basic_string_view<Char>) -> codepoint_forward_view<Char>;

// Latin-1 and UTF-32 can just read the string one character at a time.
template <class Char>
class codepoint_char_forward_view {
	private:
		using iterator_type = std::basic_string_view<Char>::const_iterator;

	public:
		explicit constexpr codepoint_char_forward_view(std::basic_string_view<Char> view) :
				pos_{view.begin()},
				end_{view.end()} {}

		[[nodiscard]] constexpr auto eof() const -> bool { return pos_ == end_; }
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		constexpr auto read() -> char32_t { return static_cast<char32_t>(*pos_++); }

	private:
		iterator_type pos_;
		iterator_type end_;
};

template <>
class codepoint_forward_view<char> : public codepoint_char_forward_view<char> {
		using codepoint_char_forward_view<char>::codepoint_char_forward_view;
};

template <>
class codepoint_forward_view<char32_t> : public codepoint_char_forward_view<char32_t> {
		using codepoint_char_forward_view<char32_t>::codepoint_char_forward_view;
};

// UTF-8 forward iterator reader
template <class Char>
class codepoint_utf8_forward_view {
	private:
		using iterator_type = std::basic_string_view<Char>::const_iterator;

	public:
		explicit constexpr codepoint_utf8_forward_view(std::basic_string_view<Char> view) :
				pos_{view.begin()},
				end_{view.end()} {}

		[[nodiscard]] constexpr auto eof() const -> bool { return pos_ == end_; }
		constexpr auto read() -> char32_t {
			// Check the expected length of the byte sequence from the leading byte's bit pattern
			auto sequence_length = [ & ]() -> unsigned {
				auto byte0 = std::bit_cast<std::uint8_t>(*pos_);
				if (byte0 < 0x80) {
					return 1;
				} else if (byte0 < 0xe0) {
					return 2;
				} else if (byte0 < 0xf0) {
					return 3;
				} else if (byte0 < 0xf8) {
					return 4;
				} else {
					std::unreachable();
				}
			}();

			// Replacement character for mojibake
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			if (pos_ + sequence_length > end_) {
				pos_ = end_;
				return 0xfffd;
			}

			// Dump bytes
			std::array<Char, 4> characters{};
			std::ranges::copy_n(pos_, sequence_length, characters.begin());
			pos_ += sequence_length;
			auto [ byte0, byte1, byte2, byte3 ] = std::bit_cast<std::array<std::uint8_t, 4>>(characters);

			// Convert byte sequence to codepoint
			switch (sequence_length) {
				case 1: return byte0;
				case 2: return ((byte0 & 0x1f) << 6) | (byte1 & 0x3f);
				case 3: return ((byte0 & 0x0f) << 12) | ((byte1 & 0x3f) << 6) | (byte2 & 0x3f);
				default: return ((byte0 & 0x07) << 18) | ((byte1 & 0x3f) << 12) | ((byte2 & 0x3f) << 6) | (byte3 & 0x3f);
			}
		}

	private:
		iterator_type pos_;
		iterator_type end_;
};

template <>
class codepoint_forward_view<char8_t> : public codepoint_utf8_forward_view<char8_t> {
		using codepoint_utf8_forward_view<char8_t>::codepoint_utf8_forward_view;
};

// UTF-16 forward reader
template <>
class codepoint_forward_view<char16_t> {
	private:
		using iterator_type = std::basic_string_view<char16_t>::const_iterator;

	public:
		explicit constexpr codepoint_forward_view(std::basic_string_view<char16_t> view) :
				pos_{view.begin()},
				end_{view.end()} {}

		[[nodiscard]] constexpr auto eof() const -> bool { return pos_ == end_; }

		constexpr auto read() -> char32_t {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			auto char0 = *pos_++;
			if (char0 >= 0xd800 && char0 < 0xdc00) {
				if (eof()) {
					// nb: Unpaired surrogate
					return char0;
				}
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				auto char1 = *pos_++;
				if (char1 >= 0xdc00 && char1 < 0xe000) {
					return ((char0 - 0xd800) * 0x400) + (char1 - 0xdc00) + 0x1'0000;
				} else {
					// nb: Unpaired surrogate
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					--pos_;
					return char0;
				}
			} else {
				return char0;
			}
		}

	private:
		iterator_type pos_;
		iterator_type end_;
};

// Interpolate the given character range to a `std::basic_string<Char>`. If the requested character
// type is too small to represent a codepoint, a `std::range_error` is thrown.
export template <class To, class From>
constexpr auto interpolate_string(std::basic_string_view<From> from) -> std::basic_string<To> {
	auto reader = codepoint_forward_view{from};
	std::basic_string<To> result;

	// Calculate destination length. In some cases you can calculate it in constant time but you're
	// not allowed to throw from `resize_and_overwrite` anyway. So you'd still have to check the range
	// before.
	// TODO: It may be worth looking at the generated code for common cases and seeing if it needs
	// optimization.
	auto size = [ & ]() -> std::size_t {
		auto size_reader = reader;
		std::size_t size = 0;
		while (!size_reader.eof()) {
			size += codepoint_char_range<To>{size_reader.read()}.size();
		}
		return size;
	}();

	// Write string
	result.resize_and_overwrite(size, [ & ](To* result, std::size_t /*length*/) -> std::size_t {
		auto first = result;
		while (!reader.eof()) {
			auto chunk = codepoint_char_range<To>{reader.read()};
			std::ranges::copy(chunk, result);
			result += chunk.size();
		}
		return result - first;
	});
	return result;
};

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
export template <class To, class From, std::size_t Extent, fixed_value<From[ Extent ]> Value>
constexpr auto interpolate_string(util::constant_wrapper<Value> /*cw*/) {
	constexpr auto make = []() { return interpolate_string<To>(std::basic_string_view{Value.value, Extent - 1}); };
	constexpr auto chars = [ = ]() {
		std::array<To, make().size()> result{};
		std::ranges::copy(make(), result.data());
		return result;
	}();
	return [ = ]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		constexpr To string[ chars.size() + 1 ] = {chars[ Indices ]..., 0};
		return util::cw<string>;
	}(std::make_index_sequence<chars.size()>());
};

} // namespace util
