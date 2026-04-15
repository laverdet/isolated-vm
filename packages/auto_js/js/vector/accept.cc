export module auto_js:vector.accept;
import :transfer;
import :vector.vector_of;
import std;
import util;

namespace js {

// `std::array` can only accept a tuple of statically known size
template <class Meta, std::size_t Size, class Type>
struct accept<Meta, std::array<Type, Size>>;

template <class Type, std::size_t Size>
struct accept_target_for<std::array<Type, Size>> : accept_target_for<Type> {};

template <class Meta, std::size_t Size, class Type>
struct accept<Meta, std::array<Type, Size>> : accept_value<Meta, Type> {
		using accept_type = accept_value<Meta, Type>;
		using accept_type::accept_type;

		template <std::size_t VectorSize>
		constexpr auto operator()(vector_n_tag<VectorSize> /*tag*/, auto& visit, auto&& subject) const -> std::array<Type, Size> {
			const accept_type& accept = *this;
			auto&& forward_range = util::forward_range(std::forward<decltype(subject)>(subject));
			auto iterator = forward_range.begin();
			const auto [... indices ] = util::sequence<Size>;
			return std::array<Type, Size>{
				// nb: Comma operator trick
				// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
				(util::unused(indices), visit(*iterator++, accept))...
			};
		}

		template <std::size_t TupleSize>
		constexpr auto operator()(tuple_tag<TupleSize> /*tag*/, auto& visit, auto&& subject) const -> std::array<Type, Size> {
			const accept_type& accept = *this;
			const auto [... indices ] = util::sequence<TupleSize>;
			return std::array<Type, Size>{
				// NOLINTNEXTLINE(bugprone-use-after-move)
				visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept)...,
			};
		}
};

// `std::vector` can accept all kinds of lists
template <class Meta, class Type>
struct accept<Meta, std::vector<Type>>;

template <class Type>
struct accept_target_for<std::vector<Type>> : accept_target_for<Type> {};

template <class Meta, class Type>
struct accept<Meta, std::vector<Type>> : public accept_value<Meta, Type> {
	private:
		using accept_type = accept_value<Meta, Type>;

	public:
		explicit accept(auto* transfer, auto&&... args) :
				accept_type{transfer, std::forward<decltype(args)>(args)...} {}

		// forward reference provider
		template <class To>
		constexpr auto operator()(std::type_identity<To> type, auto&& value) const -> To
			requires std::invocable<const accept_type&, std::type_identity<To>, decltype(value)> {
			return accept_type::operator()(type, std::forward<decltype(value)>(value));
		}

		constexpr auto operator()(list_tag /*tag*/, auto& visit, auto&& subject) const -> std::vector<Type> {
			const accept_type& accept = *this;
			// nb: This doesn't check for string keys, so like `Object.assign([ 1 ], { foo: 2 })` might
			// yield `[ 1, 2 ]`
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			return {
				std::from_range,
				util::forward_range(std::forward<decltype(range)>(range)) |
					std::views::transform([ & ](auto&& entry) -> Type {
						return visit.second(std::forward<decltype(entry)>(entry).second, accept);
					})
			};
		}

		constexpr auto operator()(vector_tag /*tag*/, auto& visit, auto&& subject) const -> std::vector<Type> {
			const accept_type& accept = *this;
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			return {
				std::from_range,
				util::forward_range(std::forward<decltype(range)>(range)) |
					std::views::transform([ & ](auto&& entry) -> Type {
						return visit(std::forward<decltype(entry)>(entry), accept);
					})
			};
		}

		template <std::size_t Size>
		constexpr auto operator()(tuple_tag<Size> /*tag*/, auto& visit, auto&& subject) const -> std::vector<Type> {
			const accept_type& accept = *this;
			std::vector<Type> result;
			result.reserve(Size);
			const auto [... indices ] = util::sequence<Size>;
			(..., result.emplace_back(visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept)));
			return result;
		}
};

// Dictionary's accepts a properly-tagged subject or maybe even a struct
template <class Meta, class Tag, class Entry>
struct accept<Meta, vector_of<Tag, Entry>> {
	private:
		using accept_type = accept<Meta, Entry>;

	public:
		explicit constexpr accept(auto* transfer) : accept_{transfer} {}

		constexpr auto operator()(Tag /*tag*/, auto& visit, auto&& subject) const -> vector_of<Tag, Entry> {
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			return vector_of<Tag, Entry>{
				std::from_range,
				util::forward_range(std::forward<decltype(range)>(range)) |
					std::views::transform([ & ](auto&& entry) -> Entry {
						return accept_(visit, std::forward<decltype(entry)>(entry));
					})
			};
		}

		template <std::size_t Size>
			requires(type<Tag> == type<dictionary_tag>)
		constexpr auto operator()(struct_tag<Size> /*tag*/, auto& visit, auto&& subject) const -> vector_of<Tag, Entry> {
			// nb: The value category of `subject` is forwarded to *each* visitor. Move operations should
			// keep this in mind and only move one member at time.
			auto& [... indices ] = util::sequence<Size>;
			return vector_of<Tag, Entry>{
				std::in_place,
				// NOLINTNEXTLINE(bugprone-use-after-move)
				visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept_)...,
			};
		}

		consteval static auto types(auto recursive) { return accept_type::types(recursive); }

	private:
		accept_type accept_;
};

} // namespace js
