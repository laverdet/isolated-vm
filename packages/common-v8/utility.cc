module;
#include <stdexcept>
export module ivm.v8:utility;
import v8;

export namespace ivm::iv8 {

template <class Type>
auto unmaybe(v8::Maybe<Type> handle) -> Type {
	Type just;
	if (handle.To(&just)) {
		return just;
	} else {
		throw std::logic_error("Missing `Maybe`");
	}
}

template <class Type>
auto unmaybe(v8::MaybeLocal<Type> handle) -> v8::Local<Type> {
	v8::Local<Type> local;
	if (handle.ToLocal(&local)) {
		return local;
	} else {
		throw std::logic_error("Missing `MaybeLocal`");
	}
}

} // namespace ivm::iv8
