module;
#include <memory>
export module util:memory.comparator;

namespace util {

export template <class Type>
class pointer_less {
	public:
		using is_transparent = void;

		template <class Left, class Right>
		constexpr auto operator()(const Left& left, const Right& right) const -> bool {
			return extract(left) < extract(right);
		}

	private:
		[[nodiscard]] constexpr auto extract(const Type* pointer) const -> const Type* {
			return pointer;
		}

		template <class From>
		[[nodiscard]] constexpr auto extract(const std::shared_ptr<From>& pointer) const -> const Type* {
			return pointer.get();
		}

		template <class From, class Deleter>
		[[nodiscard]] constexpr auto extract(const std::unique_ptr<From, Deleter>& pointer) const -> const Type* {
			return pointer.get();
		}
};

} // namespace util
