module;
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.utility:functional.function_ref;
import :functional.elide;
import :type_traits;

namespace util {

export template <class Signature>
class function_ref;

template <class Result, bool Nx, class... Args>
class function_ref<auto(Args...) noexcept(Nx)->Result> {
	private:
		template <class Type>
		constexpr explicit function_ref(std::type_identity<Type> /*tag*/, void* ptr) :
				invoke_{[](void* ptr, Args... args) noexcept(Nx) -> Result {
					auto& object = *static_cast<Type*>(ptr);
					return object(std::forward<Args>(args)...);
				}},
				ptr_{ptr} {}

	public:
		using signature_type = auto(Args...) noexcept(Nx) -> Result;

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr function_ref(signature_type* function) :
				function_ref{std::type_identity<signature_type>{}, static_cast<void*>(function)} {}

		template <std::invocable<Args...> Object>
			requires(type<Object> != type<function_ref>)
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr function_ref(Object& object) :
				function_ref{std::type_identity<Object>{}, static_cast<void*>(&object)} {}

		// Explicit to avoid automatic binding to temporary objects
		template <std::invocable<Args...> Object>
			requires(type<Object> != type<function_ref>)
		constexpr explicit function_ref(const Object& object) :
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				function_ref{std::type_identity<const Object>{}, const_cast<void*>(static_cast<const void*>(&object))} {}

		// Bound function constant
		template <class Object, std::invocable<Object&, Args...> Function>
			requires std::is_empty_v<Function>
		constexpr explicit function_ref(Function /*function*/, Object& bound) :
				invoke_{[](void* ptr, Args... args) noexcept(Nx) -> Result {
					auto& object = *static_cast<Object*>(ptr);
					constexpr auto function = Function{};
					return function(object, std::forward<Args>(args)...);
				}},
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				ptr_{const_cast<void*>(static_cast<const void*>(&bound))} {}

		constexpr auto operator()(Args... args) const noexcept(Nx) -> Result {
			return invoke_(ptr_, std::forward<Args>(args)...);
		}

	private:
		using invoke_type = auto(void*, Args...) noexcept(Nx) -> Result;

		invoke_type* invoke_;
		void* ptr_;
};

} // namespace util
