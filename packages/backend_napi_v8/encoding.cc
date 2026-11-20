module;
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <cassert>
module backend_napi_v8;
import :encoding;
import :lock;
import auto_js;
import std;
import util;
import v8_js;
using namespace std::string_view_literals;

namespace backend_napi_v8 {

// Lowercase label to `TextDecoder` encoding
// https://developer.mozilla.org/en-US/docs/Web/API/Encoding_API/Encodings
constexpr auto labels = util::sealed_map{
	std::in_place,
	std::pair{"unicode-1-1-utf-8"sv, "utf-8"sv},
	std::pair{"unicode11utf8"sv, "utf-8"sv},
	std::pair{"unicode20utf8"sv, "utf-8"sv},
	std::pair{"utf-8"sv, "utf-8"sv},
	std::pair{"utf8"sv, "utf-8"sv},
	std::pair{"x-unicode20utf8"sv, "utf-8"sv},
	std::pair{"866"sv, "ibm866"sv},
	std::pair{"cp866"sv, "ibm866"sv},
	std::pair{"csibm866"sv, "ibm866"sv},
	std::pair{"ibm866"sv, "ibm866"sv},
	std::pair{"csisolatin2"sv, "iso-8859-2"sv},
	std::pair{"iso-8859-2"sv, "iso-8859-2"sv},
	std::pair{"iso-ir-101"sv, "iso-8859-2"sv},
	std::pair{"iso8859-2"sv, "iso-8859-2"sv},
	std::pair{"iso88592"sv, "iso-8859-2"sv},
	std::pair{"iso_8859-2"sv, "iso-8859-2"sv},
	std::pair{"iso_8859-2:1987"sv, "iso-8859-2"sv},
	std::pair{"l2"sv, "iso-8859-2"sv},
	std::pair{"latin2"sv, "iso-8859-2"sv},
	std::pair{"csisolatin3"sv, "iso-8859-3"sv},
	std::pair{"iso-8859-3"sv, "iso-8859-3"sv},
	std::pair{"iso-ir-109"sv, "iso-8859-3"sv},
	std::pair{"iso8859-3"sv, "iso-8859-3"sv},
	std::pair{"iso88593"sv, "iso-8859-3"sv},
	std::pair{"iso_8859-3"sv, "iso-8859-3"sv},
	std::pair{"iso_8859-3:1988"sv, "iso-8859-3"sv},
	std::pair{"l3"sv, "iso-8859-3"sv},
	std::pair{"latin3"sv, "iso-8859-3"sv},
	std::pair{"csisolatin4"sv, "iso-8859-4"sv},
	std::pair{"iso-8859-4"sv, "iso-8859-4"sv},
	std::pair{"iso-ir-110"sv, "iso-8859-4"sv},
	std::pair{"iso8859-4"sv, "iso-8859-4"sv},
	std::pair{"iso88594"sv, "iso-8859-4"sv},
	std::pair{"iso_8859-4"sv, "iso-8859-4"sv},
	std::pair{"iso_8859-4:1988"sv, "iso-8859-4"sv},
	std::pair{"l4"sv, "iso-8859-4"sv},
	std::pair{"latin4"sv, "iso-8859-4"sv},
	std::pair{"csisolatincyrillic"sv, "iso-8859-5"sv},
	std::pair{"cyrillic"sv, "iso-8859-5"sv},
	std::pair{"iso-8859-5"sv, "iso-8859-5"sv},
	std::pair{"iso-ir-144"sv, "iso-8859-5"sv},
	std::pair{"iso8859-5"sv, "iso-8859-5"sv},
	std::pair{"iso88595"sv, "iso-8859-5"sv},
	std::pair{"iso_8859-5"sv, "iso-8859-5"sv},
	std::pair{"iso_8859-5:1988"sv, "iso-8859-5"sv},
	std::pair{"arabic"sv, "iso-8859-6"sv},
	std::pair{"asmo-708"sv, "iso-8859-6"sv},
	std::pair{"csiso88596e"sv, "iso-8859-6"sv},
	std::pair{"csiso88596i"sv, "iso-8859-6"sv},
	std::pair{"csisolatinarabic"sv, "iso-8859-6"sv},
	std::pair{"ecma-114"sv, "iso-8859-6"sv},
	std::pair{"iso-8859-6"sv, "iso-8859-6"sv},
	std::pair{"iso-8859-6-e"sv, "iso-8859-6"sv},
	std::pair{"iso-8859-6-i"sv, "iso-8859-6"sv},
	std::pair{"iso-ir-127"sv, "iso-8859-6"sv},
	std::pair{"iso8859-6"sv, "iso-8859-6"sv},
	std::pair{"iso88596"sv, "iso-8859-6"sv},
	std::pair{"iso_8859-6"sv, "iso-8859-6"sv},
	std::pair{"iso_8859-6:1987"sv, "iso-8859-6"sv},
	std::pair{"csisolatingreek"sv, "iso-8859-7"sv},
	std::pair{"ecma-118"sv, "iso-8859-7"sv},
	std::pair{"elot_928"sv, "iso-8859-7"sv},
	std::pair{"greek"sv, "iso-8859-7"sv},
	std::pair{"greek8"sv, "iso-8859-7"sv},
	std::pair{"iso-8859-7"sv, "iso-8859-7"sv},
	std::pair{"iso-ir-126"sv, "iso-8859-7"sv},
	std::pair{"iso8859-7"sv, "iso-8859-7"sv},
	std::pair{"iso88597"sv, "iso-8859-7"sv},
	std::pair{"iso_8859-7"sv, "iso-8859-7"sv},
	std::pair{"iso_8859-7:1987"sv, "iso-8859-7"sv},
	std::pair{"sun_eu_greek"sv, "iso-8859-7"sv},
	std::pair{"csiso88598e"sv, "iso-8859-8"sv},
	std::pair{"csisolatinhebrew"sv, "iso-8859-8"sv},
	std::pair{"hebrew"sv, "iso-8859-8"sv},
	std::pair{"iso-8859-8"sv, "iso-8859-8"sv},
	std::pair{"iso-8859-8-e"sv, "iso-8859-8"sv},
	std::pair{"iso-ir-138"sv, "iso-8859-8"sv},
	std::pair{"iso8859-8"sv, "iso-8859-8"sv},
	std::pair{"iso88598"sv, "iso-8859-8"sv},
	std::pair{"iso_8859-8"sv, "iso-8859-8"sv},
	std::pair{"iso_8859-8:1988"sv, "iso-8859-8"sv},
	std::pair{"visual"sv, "iso-8859-8"sv},
	std::pair{"csiso88598i"sv, "iso-8859-8-i"sv},
	std::pair{"iso-8859-8-i"sv, "iso-8859-8-i"sv},
	std::pair{"logical"sv, "iso-8859-8-i"sv},
	std::pair{"csisolatin6"sv, "iso-8859-10"sv},
	std::pair{"iso-8859-10"sv, "iso-8859-10"sv},
	std::pair{"iso-ir-157"sv, "iso-8859-10"sv},
	std::pair{"iso8859-10"sv, "iso-8859-10"sv},
	std::pair{"iso885910"sv, "iso-8859-10"sv},
	std::pair{"l6"sv, "iso-8859-10"sv},
	std::pair{"latin6"sv, "iso-8859-10"sv},
	std::pair{"iso-8859-13"sv, "iso-8859-13"sv},
	std::pair{"iso8859-13"sv, "iso-8859-13"sv},
	std::pair{"iso885913"sv, "iso-8859-13"sv},
	std::pair{"iso-8859-14"sv, "iso-8859-14"sv},
	std::pair{"iso8859-14"sv, "iso-8859-14"sv},
	std::pair{"iso885914"sv, "iso-8859-14"sv},
	std::pair{"csisolatin9"sv, "iso-8859-15"sv},
	std::pair{"iso-8859-15"sv, "iso-8859-15"sv},
	std::pair{"iso8859-15"sv, "iso-8859-15"sv},
	std::pair{"iso885915"sv, "iso-8859-15"sv},
	std::pair{"iso_8859-15"sv, "iso-8859-15"sv},
	std::pair{"l9"sv, "iso-8859-15"sv},
	std::pair{"iso-8859-16"sv, "iso-8859-16"sv},
	std::pair{"cskoi8r"sv, "koi8-r"sv},
	std::pair{"koi"sv, "koi8-r"sv},
	std::pair{"koi8"sv, "koi8-r"sv},
	std::pair{"koi8-r"sv, "koi8-r"sv},
	std::pair{"koi8_r"sv, "koi8-r"sv},
	std::pair{"koi8-ru"sv, "koi8-u"sv},
	std::pair{"koi8-u"sv, "koi8-u"sv},
	std::pair{"csmacintosh"sv, "macintosh"sv},
	std::pair{"mac"sv, "macintosh"sv},
	std::pair{"macintosh"sv, "macintosh"sv},
	std::pair{"x-mac-roman"sv, "macintosh"sv},
	std::pair{"dos-874"sv, "windows-874"sv},
	std::pair{"iso-8859-11"sv, "windows-874"sv},
	std::pair{"iso8859-11"sv, "windows-874"sv},
	std::pair{"iso885911"sv, "windows-874"sv},
	std::pair{"tis-620"sv, "windows-874"sv},
	std::pair{"windows-874"sv, "windows-874"sv},
	std::pair{"cp1250"sv, "windows-1250"sv},
	std::pair{"windows-1250"sv, "windows-1250"sv},
	std::pair{"x-cp1250"sv, "windows-1250"sv},
	std::pair{"cp1251"sv, "windows-1251"sv},
	std::pair{"windows-1251"sv, "windows-1251"sv},
	std::pair{"x-cp1251"sv, "windows-1251"sv},
	std::pair{"ansi_x3.4-1968"sv, "windows-1252"sv},
	std::pair{"ascii"sv, "windows-1252"sv},
	std::pair{"cp1252"sv, "windows-1252"sv},
	std::pair{"cp819"sv, "windows-1252"sv},
	std::pair{"csisolatin1"sv, "windows-1252"sv},
	std::pair{"ibm819"sv, "windows-1252"sv},
	std::pair{"iso-8859-1"sv, "windows-1252"sv},
	std::pair{"iso-ir-100"sv, "windows-1252"sv},
	std::pair{"iso8859-1"sv, "windows-1252"sv},
	std::pair{"iso88591"sv, "windows-1252"sv},
	std::pair{"iso_8859-1"sv, "windows-1252"sv},
	std::pair{"iso_8859-1:1987"sv, "windows-1252"sv},
	std::pair{"l1"sv, "windows-1252"sv},
	std::pair{"latin1"sv, "windows-1252"sv},
	std::pair{"us-ascii"sv, "windows-1252"sv},
	std::pair{"windows-1252"sv, "windows-1252"sv},
	std::pair{"x-cp1252"sv, "windows-1252"sv},
	std::pair{"cp1253"sv, "windows-1253"sv},
	std::pair{"windows-1253"sv, "windows-1253"sv},
	std::pair{"x-cp1253"sv, "windows-1253"sv},
	std::pair{"cp1254"sv, "windows-1254"sv},
	std::pair{"csisolatin5"sv, "windows-1254"sv},
	std::pair{"iso-8859-9"sv, "windows-1254"sv},
	std::pair{"iso-ir-148"sv, "windows-1254"sv},
	std::pair{"iso8859-9"sv, "windows-1254"sv},
	std::pair{"iso88599"sv, "windows-1254"sv},
	std::pair{"iso_8859-9"sv, "windows-1254"sv},
	std::pair{"iso_8859-9:1989"sv, "windows-1254"sv},
	std::pair{"l5"sv, "windows-1254"sv},
	std::pair{"latin5"sv, "windows-1254"sv},
	std::pair{"windows-1254"sv, "windows-1254"sv},
	std::pair{"x-cp1254"sv, "windows-1254"sv},
	std::pair{"cp1255"sv, "windows-1255"sv},
	std::pair{"windows-1255"sv, "windows-1255"sv},
	std::pair{"x-cp1255"sv, "windows-1255"sv},
	std::pair{"cp1256"sv, "windows-1256"sv},
	std::pair{"windows-1256"sv, "windows-1256"sv},
	std::pair{"x-cp1256"sv, "windows-1256"sv},
	std::pair{"cp1257"sv, "windows-1257"sv},
	std::pair{"windows-1257"sv, "windows-1257"sv},
	std::pair{"x-cp1257"sv, "windows-1257"sv},
	std::pair{"cp1258"sv, "windows-1258"sv},
	std::pair{"windows-1258"sv, "windows-1258"sv},
	std::pair{"x-cp1258"sv, "windows-1258"sv},
	std::pair{"x-mac-cyrillic"sv, "x-mac-cyrillic"sv},
	std::pair{"x-mac-ukrainian"sv, "x-mac-cyrillic"sv},
	std::pair{"chinese"sv, "gbk"sv},
	std::pair{"csgb2312"sv, "gbk"sv},
	std::pair{"csiso58gb231280"sv, "gbk"sv},
	std::pair{"gb2312"sv, "gbk"sv},
	std::pair{"gb_2312"sv, "gbk"sv},
	std::pair{"gb_2312-80"sv, "gbk"sv},
	std::pair{"gbk"sv, "gbk"sv},
	std::pair{"iso-ir-58"sv, "gbk"sv},
	std::pair{"x-gbk"sv, "gbk"sv},
	std::pair{"gb18030"sv, "gb18030"sv},
	std::pair{"big5"sv, "big5"sv},
	std::pair{"big5-hkscs"sv, "big5"sv},
	std::pair{"cn-big5"sv, "big5"sv},
	std::pair{"csbig5"sv, "big5"sv},
	std::pair{"x-x-big5"sv, "big5"sv},
	std::pair{"cseucpkdfmtjapanese"sv, "euc-jp"sv},
	std::pair{"euc-jp"sv, "euc-jp"sv},
	std::pair{"x-euc-jp"sv, "euc-jp"sv},
	std::pair{"csiso2022jp"sv, "iso-2022-jp"sv},
	std::pair{"iso-2022-jp"sv, "iso-2022-jp"sv},
	std::pair{"csshiftjis"sv, "shift_jis"sv},
	std::pair{"ms932"sv, "shift_jis"sv},
	std::pair{"ms_kanji"sv, "shift_jis"sv},
	std::pair{"shift-jis"sv, "shift_jis"sv},
	std::pair{"shift_jis"sv, "shift_jis"sv},
	std::pair{"sjis"sv, "shift_jis"sv},
	std::pair{"windows-31j"sv, "shift_jis"sv},
	std::pair{"x-sjis"sv, "shift_jis"sv},
	std::pair{"cseuckr"sv, "euc-kr"sv},
	std::pair{"csksc56011987"sv, "euc-kr"sv},
	std::pair{"euc-kr"sv, "euc-kr"sv},
	std::pair{"iso-ir-149"sv, "euc-kr"sv},
	std::pair{"korean"sv, "euc-kr"sv},
	std::pair{"ks_c_5601-1987"sv, "euc-kr"sv},
	std::pair{"ks_c_5601-1989"sv, "euc-kr"sv},
	std::pair{"ksc5601"sv, "euc-kr"sv},
	std::pair{"ksc_5601"sv, "euc-kr"sv},
	std::pair{"windows-949"sv, "euc-kr"sv},
	std::pair{"unicodefffe"sv, "utf-16be"sv},
	std::pair{"utf-16be"sv, "utf-16be"sv},
	std::pair{"csunicode"sv, "utf-16le"sv},
	std::pair{"iso-10646-ucs-2"sv, "utf-16le"sv},
	std::pair{"ucs-2"sv, "utf-16le"sv},
	std::pair{"unicode"sv, "utf-16le"sv},
	std::pair{"unicodefeff"sv, "utf-16le"sv},
	std::pair{"utf-16"sv, "utf-16le"sv},
	std::pair{"utf-16le"sv, "utf-16le"sv},
	std::pair{"x-user-defined"sv, "x-user-defined"sv},
};

// Lowercase canonical `TextDecoder` label to Unicode encoding
// https://encoding.spec.whatwg.org/#x-user-defined-decoder
constexpr auto encodings = util::sealed_map{
	std::in_place,
	std::pair{"utf-8"sv, "UTF-8"},
	std::pair{"ibm866"sv, "IBM866"},
	std::pair{"iso-8859-2"sv, "ISO-8859-2"},
	std::pair{"iso-8859-3"sv, "ISO-8859-3"},
	std::pair{"iso-8859-4"sv, "ISO-8859-4"},
	std::pair{"iso-8859-5"sv, "ISO-8859-5"},
	std::pair{"iso-8859-6"sv, "ISO-8859-6"},
	std::pair{"iso-8859-7"sv, "ISO-8859-7"},
	std::pair{"iso-8859-8"sv, "ISO-8859-8"},
	std::pair{"iso-8859-8-i"sv, "ISO-8859-8"},
	std::pair{"iso-8859-10"sv, "ISO-8859-10"},
	std::pair{"iso-8859-13"sv, "ISO-8859-13"},
	std::pair{"iso-8859-14"sv, "ISO-8859-14"},
	std::pair{"iso-8859-15"sv, "ISO-8859-15"},
	std::pair{"iso-8859-16"sv, "ISO-8859-16"},
	std::pair{"koi8-r"sv, "KOI8-R"},
	std::pair{"koi8-u"sv, "KOI8-U"},
	std::pair{"macintosh"sv, "macintosh"},
	std::pair{"windows-874"sv, "windows-874"},
	std::pair{"windows-1250"sv, "windows-1250"},
	std::pair{"windows-1251"sv, "windows-1251"},
	std::pair{"windows-1252"sv, "windows-1252"},
	std::pair{"windows-1253"sv, "windows-1253"},
	std::pair{"windows-1254"sv, "windows-1254"},
	std::pair{"windows-1255"sv, "windows-1255"},
	std::pair{"windows-1256"sv, "windows-1256"},
	std::pair{"windows-1257"sv, "windows-1257"},
	std::pair{"windows-1258"sv, "windows-1258"},
	std::pair{"x-mac-cyrillic"sv, "x-mac-cyrillic"},
	std::pair{"gbk"sv, "gb18030"},
	std::pair{"gb18030"sv, "gb18030"},
	std::pair{"big5"sv, "Big5"},
	std::pair{"euc-jp"sv, "EUC-JP"},
	std::pair{"iso-2022-jp"sv, "ISO-2022-JP"},
	std::pair{"shift_jis"sv, "Shift_JIS"},
	std::pair{"euc-kr"sv, "EUC-KR"},
	std::pair{"utf-16be"sv, "UTF-16BE"},
	std::pair{"utf-16le"sv, "UTF-16LE"},
	std::pair{"x-user-defined"sv, nullptr},
};

// string_reader_handle
string_reader_handle::string_reader_handle(std::span<char> label, bool fatal, bool ignore_bom) :
		encoding_{[ & ] -> std::string_view {
			static_assert(max_label_size == std::ranges::max(labels | std::views::transform([](const auto& entry) { return entry.first.size(); })));
			std::ranges::transform(label, label.begin(), [](char character) -> char {
				return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
			});
			const auto view = std::string_view{label.data(), label.size()};
			const auto* entry = label.size() > max_label_size ? nullptr : labels.find(view);
			if (entry == nullptr) {
				throw js::range_error{"The encoding label provided ('{}') is invalid", view};
			}
			return entry->second;
		}()},
		converter_{[ & ] -> UConverter* {
			const auto* entry = encodings.find(encoding_);
			assert(entry != nullptr);
			const auto* name = entry->second;
			if (name == nullptr) {
				return nullptr;
			}
			auto status = U_ZERO_ERROR;
			auto* converter = ucnv_open(name, &status);
			if (U_FAILURE(status)) {
				throw js::runtime_error{u"Failed to open ICU converter"};
			}
			return converter;
		}()},
		utf8_{encoding_ == "utf-8"},
		unicode_{utf8_ || encoding_.starts_with("utf-16")},
		fatal_{fatal},
		ignore_bom_{ignore_bom} {
	if (fatal && converter_ != nullptr) {
		auto status = U_ZERO_ERROR;
		ucnv_setToUCallBack(converter_.get(), UCNV_TO_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);
		assert(U_SUCCESS(status));
	}
}

// https://encoding.spec.whatwg.org/#dom-textdecoder-decode
auto string_reader_handle::read(const realm_scope& lock, std::span<const char> input, bool flush) -> js::forward<v8::Local<v8::Value>> {
	auto* isolate = lock.isolate();

	// The previous call flushed, so this is a new stream
	const auto fresh = !pending_;
	if (fresh) {
		bom_seen_ = false;
	}
	pending_ = !flush;

	// v8 decodes a complete UTF-8 buffer faster than ICU, with the same replacement behavior
	if (utf8_ && !fatal_ && fresh && flush) {
		constexpr auto bom = "\xef\xbb\xbf"sv;
		if (!ignore_bom_ && input.size() >= bom.size() && std::ranges::equal(input.first(bom.size()), bom)) {
			input = input.subspan(bom.size());
		}
		auto string = js::iv8::unmaybe(v8::String::NewFromUtf8(isolate, input.data(), v8::NewStringType::kNormal, static_cast<int>(input.size())));
		return js::forward{string};
	}

	auto output = std::u16string{};
	if (converter_ == nullptr) {
		// https://encoding.spec.whatwg.org/#x-user-defined-decoder
		output.resize_and_overwrite(input.size(), [ & ](char16_t* data, std::size_t /*capacity*/) -> std::size_t {
			std::ranges::transform(input, data, [](char byte) -> char16_t {
				auto value = static_cast<char16_t>(static_cast<unsigned char>(byte));
				return value < 0x80 ? value : static_cast<char16_t>(0xf780 + value - 0x80);
			});
			return input.size();
		});
	} else {
		// No converter emits more than one UTF-16 unit per input byte, so the output fits in the chunk size
		// plus the bytes ICU is still holding from the previous chunk.
		auto status = U_ZERO_ERROR;
		const auto pending = ucnv_toUCountPending(converter_.get(), &status);
		assert(U_SUCCESS(status));
		output.resize_and_overwrite(input.size() + pending, [ & ](char16_t* data, std::size_t capacity) -> std::size_t {
			const auto* source = input.data();
			auto* target = data;
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			ucnv_toUnicode(converter_.get(), &target, data + capacity, &source, source + input.size(), nullptr, flush, &status);
			return static_cast<std::size_t>(target - data);
		});
		assert(status != U_BUFFER_OVERFLOW_ERROR);
		// ICU resets after a successful flush. This also covers a flush which failed.
		if (flush) {
			ucnv_reset(converter_.get());
		}
		if (U_FAILURE(status) != 0) {
			return js::forward{v8::Undefined(isolate)};
		}
	}

	// https://encoding.spec.whatwg.org/#concept-td-serialize
	if (unicode_ && !ignore_bom_ && !bom_seen_ && !output.empty()) {
		bom_seen_ = true;
		if (output.front() == char16_t{0xfeff}) {
			return js::forward{js::transfer_in<v8::Local<v8::String>>(std::u16string_view{output}.substr(1), lock)};
		}
	}
	return js::forward{js::transfer_in<v8::Local<v8::String>>(std::move(output), lock)};
}

auto string_reader_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<string_reader_handle>> {
	auto constructor = [](js::iv8::value_of<js::string_tag> label, bool fatal, bool ignore_bom) -> string_reader_handle {
		return label.view_string([ & ](auto label) -> string_reader_handle {
			return string_reader_handle{label, fatal, ignore_bom};
		});
	};
	return lock->environment().class_template(
		std::type_identity<string_reader_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"StringReader">, util::fn<+constructor>},
			js::class_getter{util::cw<"encoding">, util::fn<&string_reader_handle::encoding>},
			js::class_method{util::cw<"read">, util::fn<&string_reader_handle::read>},
		}
	);
}

// string_writer_handle
auto string_writer_handle::write(const realm_scope& lock, js::forward<v8::Local<v8::String>> input, bool flush, std::optional<uint8_array> destination) -> string_writer_result {
	auto* isolate = lock.isolate();
	auto string = *input;

	// A lead surrogate held from the previous write is completed, or replaced, by this one
	const auto prepended = pending_ != 0;
	if (prepended) {
		const auto* data = reinterpret_cast<const std::uint16_t*>(&pending_);
		auto pending = js::iv8::unmaybe(v8::String::NewFromTwoByte(isolate, data, v8::NewStringType::kNormal, 1));
		string = v8::String::Concat(isolate, pending, string);
		pending_ = 0;
	}

	// Without a flush, a lead surrogate at the end waits for the next write, which may complete the pair
	auto held = false;
	if (const auto length = static_cast<std::uint32_t>(string->Length()); !flush && length > 0) {
		auto last = std::uint16_t{};
		string->Write(isolate, length - 1, 1, &last);
		if (last >= 0xd800 && last <= 0xdbff) {
			pending_ = static_cast<char16_t>(last);
			held = true;
		}
	}

	// U+FFFD is three bytes like the lone surrogate it replaces, so `Utf8Length` is exact. Dropping the
	// held surrogate from the capacity makes `WriteUtf8` stop before it.
	constexpr auto flags = v8::String::WriteFlags::kReplaceInvalidUtf8;
	const auto utf8_length = string->Utf8Length(isolate) - (held ? 3 : 0);
	auto result = string_writer_result{};
	const auto write = [ & ](char* data, std::size_t capacity) -> void {
		auto read = std::size_t{0};
		result.written = static_cast<std::int32_t>(string->WriteUtf8(isolate, data, capacity, flags, &read));
		// The prepended surrogate is not part of the input, so it doesn't count as read
		result.read = static_cast<std::int32_t>(read) - (prepended && read > 0 ? 1 : 0);
	};
	if (destination) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		auto* data = static_cast<char*>((*destination)->Buffer()->Data()) + (*destination)->ByteOffset();
		write(data, std::min((*destination)->ByteLength(), utf8_length));
		result.destination = js::forward{*destination};
	} else {
		auto backing_store = v8::ArrayBuffer::NewBackingStore(isolate, utf8_length, v8::BackingStoreInitializationMode::kUninitialized);
		write(static_cast<char*>(backing_store->Data()), utf8_length);
		assert(result.written == static_cast<std::int32_t>(utf8_length));
		auto buffer = v8::ArrayBuffer::New(isolate, std::move(backing_store));
		result.destination = js::forward{v8::Uint8Array::New(buffer, 0, utf8_length)};
	}
	return result;
}

auto string_writer_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<string_writer_handle>> {
	auto constructor = []() -> string_writer_handle { return string_writer_handle{}; };
	return lock->environment().class_template(
		std::type_identity<string_writer_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"StringWriter">, util::fn<+constructor>},
			js::class_method{util::cw<"write">, util::fn<&string_writer_handle::write>},
		}
	);
}

} // namespace backend_napi_v8
