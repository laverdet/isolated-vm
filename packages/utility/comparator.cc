module;
#include <utility>
export module ivm.utility:comparator;

namespace util {

export template <class Type>
struct address_predicate {
	public:
		explicit address_predicate(const Type& value) :
				value_{&value} {}

		auto operator()(const Type& right) const -> bool {
			return value_ == &right;
		}

	private:
		const Type* value_;
};

export template <class Hash>
struct hash_comparator {
		using is_transparent = void;

		template <class Left, class Right>
		auto operator()(const Left& left, const Right& right) const -> bool {
			return Hash{}(left) == Hash{}(right);
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
