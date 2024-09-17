module;
#include <string>
#include <type_traits>
export module ivm.v8:string;
import :handle;
import v8;

namespace ivm::iv8 {

export class string : public v8::String {
	public:
		[[nodiscard]] auto materialize(std::type_identity<std::string> tag, handle_env env) const -> std::string;
		[[nodiscard]] auto materialize(std::type_identity<std::u16string> tag, handle_env env) const -> std::u16string;
		static auto Cast(v8::Data* data) -> string*;
		static auto make(v8::Isolate* isolate, const std::string& string) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, const std::u16string& string) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String>;
};

} // namespace ivm::iv8
