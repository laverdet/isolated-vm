module;
#include <utility>
export module ivm.utility:comparator;

export template <class Operation, class Fn>
class mapped_comparator {
	public:
		explicit constexpr mapped_comparator(Operation&& operation, Fn&& fn) :
				operation{std::forward<decltype(operation)>(operation)},
				fn{std::forward<decltype(fn)>(fn)} {}

		constexpr auto operator()(const auto& left, const auto& right) const {
			return Operation{}(fn(left), fn(right));
		}

	private:
		[[no_unique_address]] Operation operation;
		[[no_unique_address]] Fn fn;
};

template <class Operation, class Fn>
mapped_comparator(Operation&&, Fn&&) -> mapped_comparator<Operation, Fn>;
