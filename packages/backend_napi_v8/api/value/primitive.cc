module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;

namespace isolated_vm {
using namespace js;

// value & primitive
auto value_for_value::is_number() const -> bool {
	return cast_in(*this)->IsNumber();
}

auto value_for_value::is_string() const -> bool {
	return cast_in(*this)->IsString();
}

auto value_for_value::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsPrimitive()) {
		return value_of<primitive_tag>::from(*this).inspect();
	} else {
		return value_of<object_tag>::from(*this).inspect();
	}
}

auto value_for_primitive::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsUndefined()) {
		return value_typeof::undefined;
	} else if (subject->IsNull()) {
		return value_typeof::null;
	} else if (subject->IsNumber()) {
		return value_of<number_tag>::from(*this).inspect();
	} else if (subject->IsName()) {
		return value_of<name_tag>::from(*this).inspect();
	} else if (subject->IsBoolean()) {
		return value_typeof::boolean;
	} else if (subject->IsBigInt()) {
		return value_of<bigint_tag>::from(*this).inspect();
	} else {
		std::unreachable();
	}
}

// null & undefined
auto value_for_undefined::make(const basic_lock& lock) -> value_of<undefined_tag> {
	return cast_out(js::transfer_in<v8::Local<iv8::Undefined>>(std::monostate{}, lock.witness()));
}

auto value_for_null::make(const basic_lock& lock) -> value_of<null_tag> {
	return cast_out(js::transfer_in<v8::Local<iv8::Null>>(nullptr, lock.witness()));
}

// boolean
auto value_for_boolean::make(const basic_lock& lock, bool value) -> value_of<boolean_tag> {
	return cast_out(js::transfer_in<v8::Local<v8::Boolean>>(value, lock.witness()));
}

bound_value_for_boolean::operator bool() const {
	return js::transfer_out<bool>(cast_in(value_of{*this}), lock().witness());
}

// number
auto value_for_number::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsInt32()) {
		return value_typeof::number_i32;
	} else {
		return value_typeof::number;
	}
}

auto value_for_number::make(const basic_lock& lock, double value) -> value_of<number_tag_of<double>> {
	return value_of<number_tag_of<double>>::from(cast_out(js::transfer_in<v8::Local<v8::Number>>(value, lock.witness())));
}

auto value_for_number::make(const basic_lock& lock, std::int32_t value) -> value_of<number_tag_of<std::int32_t>> {
	return cast_out(js::transfer_in<v8::Local<v8::Int32>>(value, lock.witness()));
}

bound_value_for_number::operator double() const {
	return js::transfer_out<double>(cast_in(value_of{*this}), lock().witness());
}

bound_value_for_number::operator std::int32_t() const {
	return js::transfer_out<std::int32_t>(cast_in(value_of{*this}), lock().witness());
}

// name & string
auto value_for_name::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsString()) {
		return value_of<string_tag>::from(*this).inspect();
	} else {
		return value_typeof::symbol;
	}
}

auto value_for_string::is_latin1() const -> bool {
	return cast_in(*this)->IsOneByte();
}

auto value_for_string::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsOneByte()) {
		return value_typeof::string_latin1;
	} else {
		return value_typeof::string;
	}
}

auto value_for_string::make(const basic_lock& lock, std::u16string_view value) -> value_of<string_tag_of<char16_t>> {
	return cast_out(js::transfer_in<v8::Local<iv8::StringTwoByte>>(value, lock.witness()));
}

auto value_for_string::make(const basic_lock& lock, std::string_view value) -> value_of<string_tag_of<char>> {
	return cast_out(js::transfer_in<v8::Local<iv8::StringOneByte>>(value, lock.witness()));
}

bound_value_for_string::operator std::string() const {
	return js::transfer_out<std::string>(cast_in(value_of{*this}), lock().witness());
}

bound_value_for_string::operator std::u16string() const {
	return js::transfer_out<std::u16string>(cast_in(value_of{*this}), lock().witness());
}

// bigint
auto value_for_bigint::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	bool lossless;
	subject->Int64Value(&lossless);
	if (lossless) {
		return value_typeof::bigint_i64;
	} else {
		return value_typeof::bigint;
	}
}

auto value_for_bigint::make(const runtime_lock& lock, js::bigint value) -> value_of<bigint_tag_of<js::bigint>> {
	return value_of<bigint_tag_of<js::bigint>>::from(cast_out(js::transfer_in<v8::Local<v8::BigInt>>(std::move(value), lock.witness())));
}

auto value_for_bigint::make(const basic_lock& lock, std::int64_t value) -> value_of<bigint_tag_of<std::int64_t>> {
	return cast_out(js::transfer_in<v8::Local<iv8::BigInt64>>(value, lock.witness()));
}

bound_value_for_bigint::operator js::bigint() const {
	return js::transfer_out<js::bigint>(cast_in(value_of{*this}), lock().witness());
}

bound_value_for_bigint::operator std::int64_t() const {
	return js::transfer_out<std::int64_t>(cast_in(value_of{*this}), lock().witness());
}

} // namespace isolated_vm
