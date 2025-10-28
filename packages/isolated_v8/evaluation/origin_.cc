module;
#include <optional>
#include <tuple>
export module isolated_v8:evaluation_origin;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export struct source_location {
		int line{};
		int column{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"line">, &source_location::line},
			js::struct_member{util::cw<"column">, &source_location::column},
		};
};

export struct source_origin {
		std::optional<js::string_t> name;
		std::optional<source_location> location;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"name">, &source_origin::name},
			js::struct_member{util::cw<"location">, &source_origin::location},
		};
};

export struct source_required_name {
		js::string_t name;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"name">, &source_required_name::name},
		};
};

} // namespace isolated_v8
