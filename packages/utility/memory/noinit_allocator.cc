module;
#include "shim/macro.h"
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
export module util:memory.noinit_allocator;

namespace util {

// Allocator which skips default construction of trivial types during runtime.
export template <class Type, class Allocator = std::allocator<Type>>
class noinit_allocator {
	public:
		template <class, class> friend class noinit_allocator;
		using value_type = Type;
		using pointer = value_type*;

		template <class T>
		struct rebind {
				using other = noinit_allocator<T, typename std::allocator_traits<Allocator>::template rebind_alloc<T>>;
		};

		noinit_allocator() = default;

		template <class T>
		constexpr explicit noinit_allocator(const noinit_allocator<T>& allocator) :
				allocator_{allocator.allocator_} {}

		auto operator==(const noinit_allocator&) const -> bool = default;

		constexpr auto allocate(std::size_t count) -> pointer {
			return allocator_.allocate(count);
		}

		constexpr auto construct(pointer ptr, auto&&... args) -> void
			requires std::constructible_from<Type, decltype(args)...> {
			auto construct = [ & ]() constexpr -> void {
				std::allocator_traits<Allocator>::construct(allocator_, ptr, std::forward<decltype(args)>(args)...);
			};
			if consteval {
				construct();
			} else {
				if constexpr (sizeof...(args) > 0) {
					construct();
				} else {
					static_assert(std::is_trivially_constructible_v<Type>);
				}
			}
		}

		constexpr auto deallocate(pointer ptr, std::size_t count) -> void {
			allocator_.deallocate(ptr, count);
		}

		constexpr auto destroy(pointer ptr) -> void {
			std::allocator_traits<Allocator>::destroy(allocator_, ptr);
		}

	private:
		NO_UNIQUE_ADDRESS Allocator allocator_;
};

} // namespace util
