module;
#include <unicode/ucnv.h>
export module backend_napi_v8:encoding;
import :lock;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// Result of `StringWriter.prototype.write`
struct string_writer_result {
		std::int32_t read = 0;
		std::int32_t written = 0;
		js::forward<v8::Local<v8::Uint8Array>> destination;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"destination">, &string_writer_result::destination},
			js::struct_member{util::cw<"read">, &string_writer_result::read},
			js::struct_member{util::cw<"written">, &string_writer_result::written},
		};
};

// `StringReader` handle, backend of `TextDecoder`
export class string_reader_handle {
	public:
		// Trims the label and copies it into bounded storage
		template <class Char>
		string_reader_handle(std::basic_string_view<Char> label, bool fatal, bool ignore_bom) :
				string_reader_handle{
					[ & ] -> label_storage {
						constexpr auto whitespace_chars = std::array<Char, 5>{'\t', '\n', '\f', '\r', ' '};
						const auto whitespace = std::basic_string_view<Char>{whitespace_chars.data(), whitespace_chars.size()};
						const auto narrow = [](Char character) -> char {
							return static_cast<std::make_unsigned_t<Char>>(character) > 0x7f ? '?' : static_cast<char>(character);
						};
						const auto begin = label.find_first_not_of(whitespace);
						const auto trimmed =
							begin == std::basic_string_view<Char>::npos
							? std::basic_string_view<Char>{}
							: label.substr(begin, label.find_last_not_of(whitespace) - begin + 1);
						return label_storage{std::from_range, trimmed | std::views::take(max_label_size + 1) | std::views::transform(narrow)};
					}(),
					fatal,
					ignore_bom
				} {}

		[[nodiscard]] auto encoding() const -> std::string_view { return encoding_; }
		auto read(const realm_scope& lock, std::span<const char> input, bool flush) -> js::forward<v8::Local<v8::Value>>;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<string_reader_handle>>;

	private:
		// Longest label in the registry, checked against the table in the implementation
		constexpr static auto max_label_size = std::size_t{19};
		using label_storage = util::inplace_vector<char, max_label_size + 1>;

		// https://encoding.spec.whatwg.org/#concept-encoding-get
		string_reader_handle(std::span<char> label, bool fatal, bool ignore_bom);

		std::string_view encoding_;
		std::unique_ptr<UConverter, util::function_constant<&ucnv_close>> converter_;
		bool utf8_;
		bool unicode_;
		bool fatal_;
		bool ignore_bom_;
		bool bom_seen_ = false;
		bool pending_ = false;
};

// `StringWriter` handle, backend of `TextEncoder`
export class string_writer_handle {
	public:
		using uint8_array = js::iv8::value_of<js::typed_array_tag_of<std::uint8_t>>;

		auto write(const realm_scope& lock, js::forward<v8::Local<v8::String>> input, bool flush, std::optional<uint8_array> destination) -> string_writer_result;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<string_writer_handle>>;

	private:
		char16_t pending_ = 0;
};

} // namespace backend_napi_v8
