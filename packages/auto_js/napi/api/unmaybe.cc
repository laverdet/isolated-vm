export module napi_js:api.unmaybe;
import std;
import v8;

namespace js::napi {

// Thrown from `unmaybe` to indicate that a direct v8 operation failed and there is an exception
// waiting in the `v8::TryCatch` of the enclosing `environment_try_catch`.
export class pending_v8_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending v8 error]"; }
};

// Unwrap a `MaybeLocal`, or throw `napi::pending_v8_error` on failure
export template <class Type>
auto unmaybe(v8::MaybeLocal<Type> maybe_value) -> v8::Local<Type> {
	v8::Local<Type> value;
	if (maybe_value.ToLocal(&value)) {
		return value;
	} else {
		throw napi::pending_v8_error{};
	}
}

// Unwrap a `Maybe`, or throw `napi::pending_v8_error` on failure
export template <class Type>
auto unmaybe(v8::Maybe<Type> maybe) -> Type {
	Type value;
	if (maybe.To(&value)) {
		return value;
	} else {
		throw napi::pending_v8_error{};
	}
}

} // namespace js::napi
