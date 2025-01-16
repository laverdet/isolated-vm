module;
#include <concepts>
#include <functional>
#include <memory>
export module ivm.utility.memory;

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

} // namespace util
