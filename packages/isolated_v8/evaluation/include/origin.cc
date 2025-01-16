module;
#include <optional>
#include <tuple>
export module isolated_v8.evaluation.origin;
import v8;
import isolated_js;

namespace isolated_v8 {

export struct source_location {
		int line{};
		int column{};
};

export struct source_origin {
		std::optional<js::string_t> name;
		std::optional<source_location> location;
};

export struct source_required_name {
		js::string_t name;
};

} // namespace isolated_v8

namespace js {
using namespace isolated_v8;

template <>
struct object_properties<source_location> {
		constexpr static auto properties = std::tuple{
			member<"line", &source_location::line>{},
			member<"column", &source_location::column>{},
		};
};

template <>
struct object_properties<source_origin> {
		constexpr static auto properties = std::tuple{
			member<"name", &source_origin::name>{},
			member<"location", &source_origin::location>{},
		};
};

template <>
struct object_properties<source_required_name> {
		constexpr static auto properties = std::tuple{
			member<"name", &source_required_name::name>{},
		};
};

} // namespace js
