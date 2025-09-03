module;
#include <utility>
export module ivm.utility:comparator;

namespace util {

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
