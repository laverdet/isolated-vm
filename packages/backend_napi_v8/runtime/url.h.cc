module;
#include <ada.h>
export module backend_napi_v8:url;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// Result of the basic URL parser, in the shape nodejs uses for its own `URL`: Ada's component offsets
// into the serialization. Each getter on the JavaScript side is then a slice of `href`, so the whole
// record crosses over as one string and a handful of numbers.
// https://url.spec.whatwg.org/#url-serializing
export struct url_record {
		ada::url_aggregator url;
		bool href_is_input;

		// Serialized URL, or `undefined` when it is identical to the input string
		[[nodiscard]] auto href() const -> std::optional<std::string_view> {
			return href_is_input ? std::nullopt : std::optional{url.get_href()};
		}

		// Components which are never `omitted`
		[[nodiscard]] auto protocol_end() const -> std::uint32_t { return url.get_components().protocol_end; }
		[[nodiscard]] auto username_end() const -> std::uint32_t { return url.get_components().username_end; }
		[[nodiscard]] auto host_start() const -> std::uint32_t { return url.get_components().host_start; }
		[[nodiscard]] auto host_end() const -> std::uint32_t { return url.get_components().host_end; }
		[[nodiscard]] auto pathname_start() const -> std::uint32_t { return url.get_components().pathname_start; }

		// Port number, rather than an offset
		[[nodiscard]] auto port() const -> std::optional<std::uint32_t> {
			const auto port = url.get_components().port;
			return port == ada::url_components::omitted ? std::nullopt : std::optional{port};
		}

		// Start of the `?`, or of the `#` or the end of the serialization when there is no query
		[[nodiscard]] auto search_start() const -> std::uint32_t {
			const auto search_start = url.get_components().search_start;
			return search_start == ada::url_components::omitted ? hash_start() : search_start;
		}

		// Start of the `#`, or the end of the serialization when there is no fragment
		[[nodiscard]] auto hash_start() const -> std::uint32_t {
			const auto hash_start = url.get_components().hash_start;
			return hash_start == ada::url_components::omitted ? static_cast<std::uint32_t>(url.get_href().size()) : hash_start;
		}

		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"hashStart">, util::fn<&url_record::hash_start>},
			js::struct_accessor{util::cw<"hostEnd">, util::fn<&url_record::host_end>},
			js::struct_accessor{util::cw<"hostStart">, util::fn<&url_record::host_start>},
			js::struct_accessor{util::cw<"href">, util::fn<&url_record::href>},
			js::struct_accessor{util::cw<"pathnameStart">, util::fn<&url_record::pathname_start>},
			js::struct_accessor{util::cw<"port">, util::fn<&url_record::port>},
			js::struct_accessor{util::cw<"protocolEnd">, util::fn<&url_record::protocol_end>},
			js::struct_accessor{util::cw<"searchStart">, util::fn<&url_record::search_start>},
			js::struct_accessor{util::cw<"usernameEnd">, util::fn<&url_record::username_end>},
		};
};

// Runs the basic URL parser on `input` against `base`. `std::nullopt` is failure.
// https://url.spec.whatwg.org/#concept-basic-url-parser
export auto parse_url(js::iv8::value_of<js::string_tag> input, std::optional<js::iv8::value_of<js::string_tag>> base) -> std::optional<url_record>;

// Reparses `href` and applies the setter for `component`
// https://url.spec.whatwg.org/#api
export auto update_url(js::iv8::value_of<js::string_tag> href, std::uint8_t component, js::iv8::value_of<js::string_tag> value) -> std::optional<url_record>;

} // namespace backend_napi_v8
