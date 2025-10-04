module;
#include <utility>
export module isolated_js:transfer.types;
import ivm.utility;
import :tag;

namespace js {

// Allows the subject or target of `transfer` to pass through a value directly without invoking
// `visit` or `accept`. For example, as a directly created element of an array.
export template <class Type, class Tag = value_tag>
struct forward : util::pointer_facade {
	public:
		explicit forward(const Type& value, Tag /*tag*/ = {}) :
				value_{value} {}
		explicit forward(Type&& value, Tag /*tag*/ = {}) :
				value_{std::move(value)} {}

		constexpr auto operator->(this auto&& self) -> auto* { return &self.value_; }

	private:
		Type value_;
};

} // namespace js
