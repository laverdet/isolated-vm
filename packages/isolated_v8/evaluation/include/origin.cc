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
struct struct_properties<source_location> {
		constexpr static auto properties = std::tuple{
			property<"line">::descriptor{struct_member{&source_location::line}},
			property<"column">::descriptor{struct_member{&source_location::column}},
		};
};

template <>
struct struct_properties<source_origin> {
		constexpr static auto properties = std::tuple{
			property<"name">::descriptor{struct_member{&source_origin::name}},
			property<"location">::descriptor{struct_member{&source_origin::location}},
		};
};

template <>
struct struct_properties<source_required_name> {
		constexpr static auto properties = std::tuple{
			property<"name">::descriptor{struct_member{&source_required_name::name}},
		};
};

} // namespace js
