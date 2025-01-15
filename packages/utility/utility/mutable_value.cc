module;
#include <utility>
export module ivm.utility.mutable_value;
import ivm.utility.facade;

namespace util {

export template <class Type>
class mutable_value : pointer_facade<mutable_value<Type>> {
	public:
		mutable_value() = default;
		// NOLINTNEXTLINE(google-explicit-constructor)
		mutable_value(Type value) :
				value_{std::move(value)} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Type&() const { return value_; }
		auto operator*() const -> Type& { return value_; }

	private:
		mutable Type value_{};
};

} // namespace util
