module;
#include <concepts>
#include <memory>
export module isolated_js:external;

namespace js {

// Holder for a runtime type-tagged external pointer.
export template <class Type>
class tagged_external {
	public:
		explicit tagged_external(Type& value) : value_{&value} {}

		static auto make(auto&&... args) -> std::unique_ptr<Type>
			requires std::constructible_from<Type, decltype(args)...> {
			return std::make_unique<Type>(std::forward<decltype(args)>(args)...);
		}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Type&() const { return *value_; }

		auto operator*() const -> Type& { return *value_; }

	private:
		Type* value_;
};

} // namespace js
