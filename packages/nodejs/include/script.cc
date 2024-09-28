module;
#include <array>
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

template <class Meta>
struct object_map<Meta, source_location_options> : object_properties<Meta, source_location_options> {
		template <auto Member>
		using property = object_properties<Meta, source_location_options>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{true, "line", property<&source_location_options::line>::accept},
			std::tuple{true, "column", property<&source_location_options::column>::accept},
		};
};

template <class Meta>
struct object_map<Meta, source_origin_options> : object_properties<Meta, source_origin_options> {
		template <auto Member>
		using property = object_properties<Meta, source_origin_options>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "name", property<&source_origin_options::name>::accept},
			std::tuple{false, "location", property<&source_origin_options::location>::accept},
		};
};

} // namespace ivm::value
