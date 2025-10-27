module;
#include <concepts>
#include <utility>
export module ivm.utility:function_ref;

namespace util {

export template <class Signature>
class function_ref;

template <class Result, class... Args>
class function_ref<auto(Args...)->Result> {
	private:
		template <class Type>
		explicit function_ref(std::type_identity<Type> /*tag*/, void* ptr) :
				invoke_{[](void* ptr, Args... args) -> Result {
					auto& object = *static_cast<Type*>(ptr);
					return object(std::forward<Args>(args)...);
				}},
				ptr_{ptr} {}

	public:
		using signature_type = auto(Args...) -> Result;

		// NOLINTNEXTLINE(google-explicit-constructor)
		function_ref(signature_type* function) :
				function_ref{std::type_identity<signature_type>{}, static_cast<void*>(function)} {}

		template <std::invocable<Args...> Object>
		// NOLINTNEXTLINE(google-explicit-constructor)
		function_ref(Object& object) :
				function_ref{std::type_identity<Object>{}, static_cast<void*>(&object)} {}

		// Explicit to avoid automatic binding to temporary objects
		template <std::invocable<Args...> Object>
		explicit function_ref(const Object& object) :
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				function_ref{std::type_identity<const Object>{}, const_cast<void*>(static_cast<const void*>(&object))} {}

		auto operator==(const function_ref&) const -> bool = default;

		auto operator()(Args... args) const -> Result {
			return invoke_(ptr_, std::forward<Args>(args)...);
		}

	private:
		using invoke_type = auto(void*, Args...) -> Result;

		invoke_type* invoke_;
		void* ptr_;
};

} // namespace util
