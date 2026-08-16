module;
#include <version>
export module util:platform.format;
import std;

namespace util {

#if __GLIBCXX__

// Claude-generated `std::format` fill to avoid gcc 16.2.0 libstdc++ requirement.

template <class Type>
struct printf_arg {
		static_assert(false, "type is not formattable by the `util::format` polyfill");
};

template <>
struct printf_arg<bool> {
		constexpr static auto spec = std::string_view{"%s"};
		constexpr static auto pass(bool value) { return std::tuple{value ? "true" : "false"}; }
};

template <>
struct printf_arg<char> {
		constexpr static auto spec = std::string_view{"%c"};
		constexpr static auto pass(char value) { return std::tuple{value}; }
};

template <class Type>
	requires std::signed_integral<Type>
struct printf_arg<Type> {
		constexpr static auto spec = std::string_view{"%lld"};
		constexpr static auto pass(Type value) { return std::tuple{static_cast<long long>(value)}; }
};

template <class Type>
	requires std::unsigned_integral<Type>
struct printf_arg<Type> {
		constexpr static auto spec = std::string_view{"%llu"};
		constexpr static auto pass(Type value) { return std::tuple{static_cast<unsigned long long>(value)}; }
};

template <class Type>
	requires(std::same_as<Type, float> || std::same_as<Type, double>)
struct printf_arg<Type> {
		constexpr static auto spec = std::string_view{"%g"};
		constexpr static auto pass(Type value) { return std::tuple{static_cast<double>(value)}; }
};

template <>
struct printf_arg<long double> {
		constexpr static auto spec = std::string_view{"%Lg"};
		constexpr static auto pass(long double value) { return std::tuple{value}; }
};

template <>
struct printf_arg<const char*> {
		constexpr static auto spec = std::string_view{"%s"};
		constexpr static auto pass(const char* value) { return std::tuple{value}; }
};

template <>
struct printf_arg<char*> : printf_arg<const char*> {};

template <>
struct printf_arg<std::string_view> {
		constexpr static auto spec = std::string_view{"%.*s"};
		constexpr static auto pass(std::string_view value) {
			return std::tuple{static_cast<int>(value.size()), value.data()};
		}
};

template <>
struct printf_arg<std::string> : printf_arg<std::string_view> {};

template <>
struct printf_arg<const void*> {
		constexpr static auto spec = std::string_view{"%p"};
		constexpr static auto pass(const void* value) { return std::tuple{value}; }
};

template <>
struct printf_arg<void*> : printf_arg<const void*> {};

template <>
struct printf_arg<std::nullptr_t> {
		constexpr static auto spec = std::string_view{"%p"};
		constexpr static auto pass(std::nullptr_t /*value*/) { return std::tuple{static_cast<const void*>(nullptr)}; }
};

// Checks the format string against the argument types and rewrites `{}` replacement fields into
// printf conversion specifiers, all at compile time. Only plain `{}` fields are supported --
// `{0}`-style indices and `{:..}` format specifiers are rejected.
export template <class... Args>
class basic_format_string {
	public:
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		template <std::size_t Size>
		consteval basic_format_string(const char (&format)[ Size ]) {
			auto out = string_.begin();
			auto put = [ & ](char character) {
				if (out == string_.end() - 1) {
					throw std::format_error{"format string exceeds polyfill capacity"};
				}
				*out++ = character;
			};
			auto emit = [ & ](std::string_view string) {
				for (auto character : string) {
					put(character);
				}
			};
			constexpr auto specs = std::array<std::string_view, sizeof...(Args)>{printf_arg<std::decay_t<Args>>::spec...};
			std::size_t argument = 0;
			auto view = std::string_view{format, Size - 1};
			for (auto it = view.begin(); it != view.end(); ++it) {
				switch (*it) {
					case '{':
						if (++it == view.end()) {
							throw std::format_error{"unmatched '{' in format string"};
						} else if (*it == '{') {
							put('{');
						} else if (*it == '}') {
							if (argument == specs.size()) {
								throw std::format_error{"too few arguments for format string"};
							}
							emit(specs[ argument++ ]);
						} else {
							throw std::format_error{"format specifiers are not supported by the `util::format` polyfill"};
						}
						break;
					case '}':
						if (++it == view.end() || *it != '}') {
							throw std::format_error{"unmatched '}' in format string"};
						}
						put('}');
						break;
					case '%':
						emit("%%");
						break;
					default:
						put(*it);
				}
			}
		}

		[[nodiscard]] constexpr auto get() const -> const char* { return string_.data(); }

	private:
		std::array<char, 256> string_{};
};

export template <class... Args>
using format_string = basic_format_string<std::type_identity_t<Args>...>;

export template <class... Args>
[[nodiscard]] auto format(format_string<Args...> format, Args&&... args) -> std::string {
	return std::apply(
		[ & ](auto... values) -> std::string {
			auto length = std::max(0, std::snprintf(nullptr, 0, format.get(), values...));
			auto result = std::string{};
			result.resize(static_cast<std::size_t>(length));
			std::snprintf(result.data(), result.size() + 1, format.get(), values...);
			return result;
		},
		std::tuple_cat(printf_arg<std::decay_t<Args>>::pass(args)...)
	);
}

#else

export template <class... Args>
using format_string = std::format_string<Args...>;

export template <class... Args>
[[nodiscard]] auto format(std::format_string<std::type_identity_t<Args>...> format, Args&&... args) -> std::string {
	return std::format(format, std::forward<Args>(args)...);
}

#endif

} // namespace util
