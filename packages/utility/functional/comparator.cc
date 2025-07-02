module;
#include <concepts>
#include <utility>
export module ivm.utility:comparator;

namespace util {

export template <class Hash>
	requires requires { typename Hash::is_transparent; }
struct hash_compare : private Hash {
		using is_transparent = Hash::is_transparent;

		auto operator()(auto&& left, auto&& right) const -> bool
			requires std::invocable<const Hash&, decltype(left)> && std::invocable<const Hash&, decltype(left)> {
			return Hash::operator()(std::forward<decltype(left)>(left)) ==
				Hash::operator()(std::forward<decltype(right)>(right));
		}
};

export template <class Operation, class Fn>
class mapped_comparator {
	public:
		constexpr mapped_comparator(auto&& operation, auto&& fn) :
				operation{std::forward<decltype(operation)>(operation)},
				fn{std::forward<decltype(fn)>(fn)} {}

		constexpr auto operator()(const auto& left, const auto& right) const {
			return operation(fn(left), fn(right));
		}

	private:
		[[no_unique_address]] Operation operation;
		[[no_unique_address]] Fn fn;
};

template <class Operation, class Fn>
mapped_comparator(Operation&&, Fn&&) -> mapped_comparator<Operation, Fn>;

} // namespace util
