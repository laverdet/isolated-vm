module;
#include <exception>
export module v8_js:unmaybe;
import v8;

namespace js::iv8 {

// Thrown from `unmaybe` to indicate that a v8 operation failed and there is an exception waiting on
// the JS stack.
export class pending_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending v8 error]"; }
};

// Unwrap a `MaybeLocal`, or throw `iv8::pending_error` on failure
export template <class Type>
auto unmaybe(v8::MaybeLocal<Type> maybe_value) -> v8::Local<Type> {
	v8::Local<Type> value;
	if (maybe_value.ToLocal(&value)) {
		return value;
	} else {
		throw iv8::pending_error{};
	}
}

// Unwrap a `Maybe`, or throw `iv8::pending_error` on failure
export template <class Type>
auto unmaybe(v8::Maybe<Type> maybe) -> Type {
	Type value;
	if (maybe.To(&value)) {
		return value;
	} else {
		throw iv8::pending_error{};
	}
}

} // namespace js::iv8
