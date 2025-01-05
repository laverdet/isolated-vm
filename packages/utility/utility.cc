module;
#include <ranges>
#include <utility>
#include <variant>
export module ivm.utility:utility;

namespace ivm::util {

// https://en.cppreference.com/w/cpp/utility/variant/visit
export template <class... Visitors>
struct overloaded : Visitors... {
		using Visitors::operator()...;
};

// Some good thoughts here. It's strange that there isn't an easier way to transform an underlying
// range.
// https://brevzin.github.io/c++/2024/05/18/range-customization/
export constexpr auto into_range(std::ranges::range auto&& range) -> decltype(auto) {
	return range;
}

export constexpr auto into_range(auto&& range) -> decltype(auto)
	requires requires() {
		{ range.into_range() };
	}
{
	return range.into_range();
}

// Structural string literal which may be used as a template parameter. The string is
// null-terminated, which is required by v8's `String::NewFromUtf8Literal`.
export template <size_t Size>
struct string_literal {
		// NOLINTNEXTLINE(google-explicit-constructor, modernize-avoid-c-arrays)
		consteval string_literal(const char (&string)[ Size ]) {
			std::copy_n(static_cast<const char*>(string), Size, payload.data());
		}

		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval operator std::string_view() const { return {payload.data(), length()}; }
		consteval auto operator<=>(const string_literal& right) const -> std::strong_ordering { return payload <=> right.payload; }
		consteval auto operator==(const string_literal& right) const -> bool { return payload == right.payload; }
		constexpr auto operator==(std::string_view string) const -> bool { return std::string_view{*this} == string; }
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		[[nodiscard]] constexpr auto data() const -> const char (&)[ Size ] { return *reinterpret_cast<const char(*)[ Size ]>(payload.data()); }
		[[nodiscard]] consteval auto length() const -> size_t { return Size - 1; }

		std::array<char, Size> payload{};
};

// Like `std::visit` but it visits with a `std::integral_constant` of the variant index. That way
// you can use the index for other structures.
template <class... Types>
constexpr auto visit_with_index_(const auto& visitor, auto&& variant) -> decltype(auto) {
	auto next = []<size_t Index>(std::integral_constant<size_t, Index> index, const auto& next, const auto& visitor, auto&& variant) constexpr -> decltype(auto) {
		if constexpr (Index == sizeof...(Types)) {
			std::unreachable();
			return next(std::integral_constant<size_t, 0>{}, next, visitor, std::forward<decltype(variant)>(variant));
		} else if (Index == variant.index()) {
			return visitor(index, std::get<Index>(std::forward<decltype(variant)>(variant)));
		} else {
			return next(std::integral_constant<size_t, Index + 1>{}, next, visitor, std::forward<decltype(variant)>(variant));
		}
	};
	return next(std::integral_constant<size_t, 0>{}, next, visitor, std::forward<decltype(variant)>(variant));
}

export template <class... Types>
constexpr auto visit_with_index(const auto& visitor, const std::variant<Types...>& variant) -> decltype(auto) {
	return visit_with_index_<Types...>(visitor, variant);
}

export template <class... Types>
constexpr auto visit_with_index(const auto& visitor, std::variant<Types...>&& variant) -> decltype(auto) {
	return visit_with_index_<Types...>(visitor, std::move(variant));
}

// `boost::noncopyable` actually prevents moving too
export class non_copyable {
	public:
		non_copyable() = default;
		non_copyable(const non_copyable&) = delete;
		non_copyable(non_copyable&&) = default;
		~non_copyable() = default;
		auto operator=(const non_copyable&) -> non_copyable& = delete;
		auto operator=(non_copyable&&) -> non_copyable& = default;
};

export class non_moveable {
	public:
		non_moveable() = default;
		non_moveable(const non_moveable&) = delete;
		non_moveable(non_moveable&&) = delete;
		~non_moveable() = default;
		auto operator=(const non_moveable&) -> non_moveable& = delete;
		auto operator=(non_moveable&&) -> non_moveable& = delete;
};

// https://en.cppreference.com/w/cpp/experimental/scope_exit
export template <class Invoke>
class scope_exit : non_copyable {
	public:
		explicit scope_exit(Invoke invoke) :
				invoke_{std::move(invoke)} {}
		scope_exit(const scope_exit&) = delete;
		~scope_exit() { invoke_(); }
		auto operator=(const scope_exit&) -> scope_exit& = delete;

	private:
		Invoke invoke_;
};

export template <class Type>
class defaulter_finalizer : non_copyable {
	public:
		explicit defaulter_finalizer(Type& value) :
				value{&value} {};
		auto operator()() -> void { *value = Type{}; }

	private:
		Type* value;
};

} // namespace ivm::util
