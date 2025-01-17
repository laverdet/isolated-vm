module;
#include <string>
#include <type_traits>
export module v8_js.string;
import v8_js.handle;
import v8;

namespace js::iv8 {

export class string : public v8::String {
	public:
		[[nodiscard]] auto materialize(std::type_identity<std::string> tag, handle_env env) const -> std::string;
		[[nodiscard]] auto materialize(std::type_identity<std::u16string> tag, handle_env env) const -> std::u16string;
		static auto Cast(v8::Data* data) -> string*;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(v8::Isolate* isolate, const char string[ Size ]) -> v8::Local<v8::String> {
			return v8::String::NewFromUtf8Literal(isolate, string);
		}

		static auto make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String>;
};

} // namespace js::iv8
