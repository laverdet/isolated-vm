module;
#include <utility>
export module ivm.utility:mutable_value;
import :facade;

namespace ivm::util {

export template <class Type>
class mutable_value : pointer_facade<mutable_value<Type>> {
	public:
		mutable_value() = default;
		mutable_value(Type value) : // NOLINT(google-explicit-constructor)
				value_{std::move(value)} {}

		operator Type&() const { return value_; } // NOLINT(google-explicit-constructor)
		auto operator*() const -> Type& { return value_; }

	private:
		mutable Type value_{};
};

} // namespace ivm::util
