module v8_js;
import :error;
import std;

namespace js::iv8 {

auto take_maybe_string(context_lock_witness lock, v8::Local<v8::Value> value) -> std::u16string {
	if (value->IsString()) {
		return transfer_out_strict<std::u16string>(value.As<v8::String>(), lock);
	} else {
		return {};
	}
};

auto take_maybe_string(context_lock_witness lock, v8::MaybeLocal<v8::Value> value) -> std::u16string {
	auto as_string = v8::Local<v8::Value>{};
	if (value.ToLocal(&as_string)) {
		return take_maybe_string(lock, as_string);
	} else {
		return {};
	}
}

// Transfer a v8 exception message into `js::error_value`, hopefully without throwing again.
auto externalize_caught_error(context_lock_witness lock, const v8::TryCatch& try_catch) -> js::error_value {
	return externalize_caught_error(lock, try_catch.Exception());
}

// Transfer an arbitrary v8 value into `js::error_value`. Probably the result of a promise.
auto externalize_caught_error(context_lock_witness lock, v8::Local<v8::Value> value) -> js::error_value {
	if (value->IsObject()) {
		auto object = value.As<v8::Object>();
		auto name = js::transfer_out_strict<std::u16string>(object->GetConstructorName(), lock);
		const auto message_key = js::transfer_in_strict<v8::Local<v8::String>>(util::cw<"message">, lock);
		auto message = take_maybe_string(lock, unmaybe(object->Get(lock.context(), message_key)));
		auto stack = take_maybe_string(lock, v8::TryCatch::StackTrace(lock.context(), object));
		return js::error_value{js::error_name::error, std::move(message), std::move(stack)};
	} else {
		auto message = transfer_out_strict<std::u16string>(unmaybe(value->ToString(lock.context())), lock);
		return js::error_value{js::error_name::error, message};
	}
}

} // namespace js::iv8
