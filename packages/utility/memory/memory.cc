module;
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>
export module ivm.utility:memory;

namespace util {

export template <class Type, class Deleter>
auto move_pointer_operation(
	std::unique_ptr<Type, Deleter> unique,
	std::invocable<Type*, std::unique_ptr<Type, Deleter>> auto function
) -> decltype(auto) {
	auto* ptr = unique.get();
	return std::invoke(std::move(function), ptr, std::move(unique));
}

export template <class Type>
auto move_pointer_operation(
	std::shared_ptr<Type> shared,
	std::invocable<Type*, std::shared_ptr<Type>> auto function
) -> decltype(auto) {
	auto* ptr = shared.get();
	return std::invoke(std::move(function), ptr, std::move(shared));
}

// https://stackoverflow.com/questions/76445860/implementation-of-stdstart-lifetime-as/76794371#76794371
export template <class Type>
	requires(std::is_trivially_copyable_v<Type> && std::is_trivially_destructible_v<Type>)
auto start_lifetime_as(void* ptr) noexcept -> auto* {
	return std::launder(static_cast<Type*>(std::memmove(ptr, ptr, sizeof(Type))));
}

} // namespace util
