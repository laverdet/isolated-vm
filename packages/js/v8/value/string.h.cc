module;
#include <string>
export module v8_js:string;
import :handle;
import :lock;
import isolated_js;
import v8;

namespace js::iv8 {

export class string
		: public v8::Local<v8::String>,
			public handle_with_isolate {
	public:
		explicit string(isolate_lock_witness lock, v8::Local<v8::String> handle) :
				v8::Local<v8::String>{handle},
				handle_with_isolate{lock} {}

		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u8string() const;
		[[nodiscard]] explicit operator std::u16string() const;

		static auto make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::u8string_view view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String>;
};

} // namespace js::iv8
