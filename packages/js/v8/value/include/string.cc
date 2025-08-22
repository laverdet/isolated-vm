module;
#include <string>
#include <type_traits>
export module v8_js.string;
import isolated_js;
import v8_js.handle;
import v8_js.lock;
import v8;

namespace js::iv8 {

export class string
		: public v8::Local<v8::String>,
			public handle_with_isolate,
			public materializable<string> {
	public:
		explicit string(const isolate_lock_witness& lock, v8::Local<v8::String> handle) :
				Local{handle},
				handle_with_isolate{lock} {}

		[[nodiscard]] auto materialize(std::type_identity<std::u16string> tag) const -> std::u16string;
		[[nodiscard]] auto materialize(std::type_identity<std::basic_string<std::byte>> tag) const -> std::basic_string<std::byte>;
		[[nodiscard]] auto materialize(std::type_identity<std::string> tag) const -> std::string;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(v8::Isolate* isolate, const char string[ Size ]) -> v8::Local<v8::String> {
			return v8::String::NewFromUtf8Literal(isolate, string);
		}

		static auto make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::basic_string_view<std::byte> view) -> v8::Local<v8::String>;
		static auto make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String>;
};

} // namespace js::iv8
