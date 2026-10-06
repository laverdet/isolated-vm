export module v8_js:typeof_of;
import :lock;
import auto_js;
import v8;

namespace js::iv8 {

// `typeof` operator. Each overload inspects only the kinds which are possible for its handle type.
export auto typeof_of(const null_lock_witness& lock, v8::Local<v8::Value> value) -> js::typeof_kind;
export auto typeof_of(const null_lock_witness& lock, v8::Local<v8::Primitive> value) -> js::typeof_kind;
export auto typeof_of(const null_lock_witness& lock, v8::Local<v8::Object> value) -> js::typeof_kind;

} // namespace js::iv8
