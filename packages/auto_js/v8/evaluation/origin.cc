module;
#include <optional>
#include <string>
export module v8_js:evaluation.origin;
import auto_js;
import util;

namespace js::iv8 {

export struct source_location {
		int line{};
		int column{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"line">, &source_location::line},
			js::struct_member{util::cw<"column">, &source_location::column},
		};
};

export struct source_origin {
		std::optional<std::u16string> name;
		std::optional<source_location> location;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"name">, &source_origin::name},
			js::struct_member{util::cw<"location">, &source_origin::location},
		};
};

} // namespace js::iv8
