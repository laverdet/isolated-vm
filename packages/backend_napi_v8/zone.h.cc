export module backend_napi_v8:zone;
import :lock;
import auto_js;
import std;
import v8_js;

namespace backend_napi_v8 {

// Runtime support for promise-tracking context data
export class zone_handle {
	public:
		zone_handle() = default;

		auto run(const realm_scope& lock, js::iv8::this_object self, js::forward<v8::Local<js::iv8::Function>> callback) -> js::forward<v8::Local<v8::Value>>;
		static auto current(const realm_scope& lock) -> js::forward<v8::Local<v8::Value>>;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<zone_handle>>;
};

} // namespace backend_napi_v8
