module v8_js;
import :typeof_of;
import auto_js;
import std;
import v8;

namespace js::iv8 {

auto typeof_of(const null_lock_witness& lock, v8::Local<v8::Value> value) -> js::typeof_kind {
	if (value->IsObject()) {
		return typeof_of(lock, value.As<v8::Object>());
	} else {
		return typeof_of(lock, value.As<v8::Primitive>());
	}
}

auto typeof_of(const null_lock_witness& /*lock*/, v8::Local<v8::Primitive> value) -> js::typeof_kind {
	if (value->IsUndefined()) {
		return js::typeof_kind::undefined;
	} else if (value->IsNull()) {
		return js::typeof_kind::null;
	} else if (value->IsString()) {
		return js::typeof_kind::string;
	} else if (value->IsNumber()) {
		return js::typeof_kind::number;
	} else if (value->IsBoolean()) {
		return js::typeof_kind::boolean;
	} else if (value->IsBigInt()) {
		return js::typeof_kind::bigint;
	} else if (value->IsSymbol()) {
		return js::typeof_kind::symbol;
	} else {
		std::unreachable();
	}
}

auto typeof_of(const null_lock_witness& /*lock*/, v8::Local<v8::Object> value) -> js::typeof_kind {
	if (value->IsFunction()) {
		return js::typeof_kind::function;
	} else {
		return js::typeof_kind::object;
	}
}

} // namespace js::iv8
