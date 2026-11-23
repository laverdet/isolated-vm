module;
#include <ada.h>
module backend_napi_v8;
import :url;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {
namespace {

// Invokes `function` with the UTF-8 bytes of `string`, which is what Ada parses
auto with_utf8(js::iv8::value_of<js::string_tag> string, const auto& function) {
	return string.view_string([ & ](auto view) { return util::with_utf8(view, function); });
}

// Runs the basic URL parser on `input` against `base`
auto parse(js::iv8::value_of<js::string_tag> input, const ada::url_aggregator* base) -> std::optional<url_record> {
	return with_utf8(input, [ & ](std::string_view utf8) -> std::optional<url_record> {
		auto result = ada::parse<ada::url_aggregator>(utf8, base);
		if (!result) {
			return std::nullopt;
		}
		const auto href_is_input = result->get_href() == utf8;
		return url_record{.url = *std::move(result), .href_is_input = href_is_input};
	});
}

} // namespace

auto parse_url(js::iv8::value_of<js::string_tag> input, std::optional<js::iv8::value_of<js::string_tag>> base) -> std::optional<url_record> {
	// The base is parsed on its own first, so that only one string is viewed at a time
	auto base_url = std::optional<url_record>{};
	if (base) {
		base_url = parse(*base, nullptr);
		if (!base_url) {
			return std::nullopt;
		}
	}
	return parse(input, base_url ? &base_url->url : nullptr);
}

auto update_url(js::iv8::value_of<js::string_tag> href, std::uint8_t component, js::iv8::value_of<js::string_tag> value) -> std::optional<url_record> {
	auto record = parse(href, nullptr);
	if (!record) {
		return std::nullopt;
	}
	const auto updated = with_utf8(value, [ & ](std::string_view utf8) -> bool {
		auto& url = record->url;
		switch (component) {
			case 0: return url.set_protocol(utf8);
			case 1: return url.set_username(utf8);
			case 2: return url.set_password(utf8);
			case 3: return url.set_host(utf8);
			case 4: return url.set_hostname(utf8);
			case 5: return url.set_port(utf8);
			case 6: return url.set_pathname(utf8);
			default: return false;
		}
	});
	if (!updated) {
		return std::nullopt;
	}
	record->href_is_input = false;
	return record;
}

} // namespace backend_napi_v8
