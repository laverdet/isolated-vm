module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;

namespace isolated_vm {
using namespace js;

auto inspect_type(const basic_lock& lock, value_of<value_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	if (subject->IsPrimitive()) {
		return inspect_type(lock, value_of<primitive_tag>::from(value));
	} else {
		std::unreachable();
	}
}

auto inspect_type(const basic_lock& lock, value_of<primitive_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	if (subject->IsNullOrUndefined()) {
		if (subject->IsNull()) {
			return value_typeof::null;
		} else {
			return value_typeof::undefined;
		}
	} else if (subject->IsNumber()) {
		return inspect_type(lock, value_of<number_tag>::from(value));
	} else if (subject->IsName()) {
		return inspect_type(lock, value_of<name_tag>::from(value));
	} else if (subject->IsBoolean()) {
		return value_typeof::boolean;
	} else if (subject->IsBigInt()) {
		return inspect_type(lock, value_of<bigint_tag>::from(value));
	} else {
		std::unreachable();
	}
}

auto inspect_type(const basic_lock& /*lock*/, value_of<number_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	if (subject->IsInt32()) {
		return value_typeof::number_i32;
	} else {
		return value_typeof::number;
	}
}

auto inspect_type(const basic_lock& lock, value_of<name_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	if (subject->IsString()) {
		return inspect_type(lock, value_of<string_tag>::from(value));
	} else {
		return value_typeof::symbol;
	}
}

auto inspect_type(const basic_lock& /*lock*/, value_of<string_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	if (subject->IsOneByte()) {
		return value_typeof::string_latin1;
	} else {
		return value_typeof::string;
	}
}

auto inspect_type(const basic_lock& /*lock*/, value_of<bigint_tag> value) -> value_typeof {
	auto subject = cast_in(value);
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	bool lossless;
	subject->Int64Value(&lossless);
	if (lossless) {
		return value_typeof::bigint_i64;
	} else {
		return value_typeof::bigint;
	}
}

} // namespace isolated_vm
