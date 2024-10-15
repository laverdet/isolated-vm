module;
#include <cstdint>
#include <optional>
#include <tuple>
export module ivm.node:script;
import ivm.isolated_v8;
import ivm.value;

namespace ivm {

export struct source_location_options {
		int32_t line{};
		int32_t column{};
};

export struct source_origin_options {
		std::optional<value::string_t> name;
		std::optional<source_location_options> location;
};

export auto make_source_origin(agent::lock& agent, const std::optional<source_origin_options>& options) -> source_origin;

} // namespace ivm

namespace ivm::value {

template <>
struct object_properties<source_location_options> {
		constexpr static auto properties = std::tuple{
			property<&source_location_options::line>{"line"},
			property<&source_location_options::column>{"column"},
		};
};

template <>
struct object_properties<source_origin_options> {
		constexpr static auto properties = std::tuple{
			property<&source_origin_options::name>{"name"},
			property<&source_origin_options::location>{"location"},
		};
};

} // namespace ivm::value
