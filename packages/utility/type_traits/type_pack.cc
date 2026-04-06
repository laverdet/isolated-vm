export module util:type_traits.type_pack;
import :type_traits.type_of;
import std;

namespace util {

// Capture a pack of types into a single type
export template <class... Types>
struct type_pack;

// Prevent `type_pack{type_pack{}}` from flattening itself via copy construction
template <class... Types>
type_pack(type_pack<Types...>) -> type_pack<type_pack<Types...>>;

// Accept any `T::type` producing structure
template <class... Types>
type_pack(Types...) -> type_pack<typename Types::type...>;

// ADL-safe pack (not exported)
template <class... Hidden>
struct type_pack_of : type_pack<typename Hidden::type...> {
		using type_pack<typename Hidden::type...>::type_pack;
};

// Prevent `type_pack_of{type_pack_of{}}` from flattening itself via copy construction
template <class... Types>
type_pack_of(type_pack_of<Types...>) -> type_pack_of<hidden_t<type_pack_of<Types...>>>;

// Accept any `T::type` producing structure
template <class... Types>
type_pack_of(Types...) -> type_pack_of<hidden_t<typename Types::type>...>;

// Capture a pack of types into a single type
export template <class... Types>
struct type_pack : std::type_identity<type_pack<Types...>> {
		consteval type_pack() = default;

		template <class... Args>
		explicit consteval type_pack(Args... /*types*/)
			requires((... && (type<typename Args::type> == type<Types>))) {}

		[[nodiscard]] consteval auto at(auto index) const { return get<index>(); }

		// Structured binding accessor
		// Explicit return type causes error:
		// "candidate template ignored: substitution failure [with Index = 0]: invalid index 0 for pack 'Types' of size 0"
		// ...even with `requires(Index < sizeof...(Types))` on the template and/or signature
		template <std::size_t Index>
		[[nodiscard]] consteval auto get() const /* -> std::type_identity<Types... [ Index ]> */ {
			// return {};
			return type<Types...[ Index ]>;
		}

		// Find the index of the given type, or `size()` if not found
		template <class Type>
		[[nodiscard]] consteval auto find(std::type_identity<Type> subject) const -> std::size_t {
			constexpr auto type_switch = [ = ]<std::size_t Index>(this const auto& self, std::integral_constant<std::size_t, Index> /*index*/) -> std::size_t {
				if constexpr (Index == sizeof...(Types)) {
					return sizeof...(Types);
				} else if constexpr (type<Types...[ Index ]> == subject) {
					return Index;
				} else {
					return self(std::integral_constant<std::size_t, Index + 1>{});
				}
			};
			return type_switch(std::integral_constant<std::size_t, 0>{});
		}

		[[nodiscard]] consteval auto size() const -> std::size_t { return sizeof...(Types); }

		// Concatenate two `type_pack`s (into a `type_pack_of`)
		template <class... Right>
		[[nodiscard]] consteval auto operator+(type_pack<Right...> /*right*/) const
			-> type_pack_of<hidden_t<Types>..., hidden_t<Right>...> {
			return {};
		}

		template <class... Right>
		[[nodiscard]] consteval auto operator==(type_pack<Right...> /*right*/) const -> bool {
			if constexpr (sizeof...(Types) == 0 && sizeof...(Right) == 0) {
				return true;
			} else if constexpr (sizeof...(Types) != sizeof...(Right)) {
				return false;
			} else {
				return ((type<Types> == type<Right>) && ...);
			}
		}
};

} // namespace util

namespace std {

// `util::type_pack` destructuring specializations
template <std::size_t Index, class... Types>
struct tuple_element<Index, util::type_pack<Types...>> {
		using type = util::type_of<util::hidden_t<Types...[ Index ]>>;
};

template <class... Types>
struct tuple_size<util::type_pack<Types...>> {
		constexpr static auto value = sizeof...(Types);
};

// `util::type_pack_of` destructuring specializations
template <std::size_t Index, class... Hidden>
struct tuple_element<Index, util::type_pack_of<Hidden...>>
		: tuple_element<Index, util::type_pack<typename Hidden::type...>> {};

template <class... Hidden>
struct tuple_size<util::type_pack_of<Hidden...>>
		: tuple_size<util::type_pack<typename Hidden::type...>> {};

} // namespace std
