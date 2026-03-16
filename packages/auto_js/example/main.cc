// demo.cc — showcases all @auto_js/napi features via a minimal "geometry" demo
// Note: Generated almost entirely by Claude
#include <napi_js_initialize.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
import auto_js;
import napi_js;
import util;

using namespace std::string_view_literals;

// ── String table ──────────────────────────────────────────────────────────────
// Each entry gets a persistent v8::String handle — no repeated v8 allocation.
constexpr auto string_literals = std::tuple{
	"b"sv,
	"center"sv,
	"g"sv,
	"height"sv,
	"r"sv,
	"radius"sv,
	"topLeft"sv,
	"type"sv,
	"width"sv,
	"x"sv,
	"y"sv,
};

// Stores persistent napi references to class constructors for object-wrap types.
constexpr auto class_names = std::tuple{"Canvas"sv};

class environment
		: public js::napi::environment,
			public js::napi::string_table<string_literals>,
			public js::napi::class_template_references<class_names> {
	public:
		using js::napi::environment::environment;
};

// ── Enum ──────────────────────────────────────────────────────────────────────
// JavaScript sees string values "north" / "south" / "east" / "west".
enum class direction : int8_t {
	north,
	south,
	east,
	west
};

namespace js {
template <>
struct enum_values<direction> {
		constexpr static auto values = std::array{
			std::pair{u"north", direction::north},
			std::pair{u"south", direction::south},
			std::pair{u"east", direction::east},
			std::pair{u"west", direction::west},
		};
};
} // namespace js

// ── Structs ───────────────────────────────────────────────────────────────────
// Direct member access — simplest form.
struct point {
		double x{};
		double y{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"x">, &point::x},
			js::struct_member{util::cw<"y">, &point::y},
		};
};

// Accessor-style — getter and setter functions instead of direct access.
struct color_rgb {
		uint32_t r{};
		uint32_t g{};
		uint32_t b{};

		auto get_r() const -> uint32_t { return r; }
		auto set_r(uint32_t v) -> void { r = v; }

		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"r">, util::fn<&color_rgb::get_r>, util::fn<&color_rgb::set_r>},
			js::struct_member{util::cw<"g">, &color_rgb::g},
			js::struct_member{util::cw<"b">, &color_rgb::b},
		};
};

// ── Discriminated union ───────────────────────────────────────────────────────
// TypeScript-style tagged unions. The "type" field selects the alternative.
struct circle {
		point center;
		double radius{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"center">, &circle::center},
			js::struct_member{util::cw<"radius">, &circle::radius},
		};
};

struct rectangle {
		point top_left;
		double width{};
		double height{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"topLeft">, &rectangle::top_left},
			js::struct_member{util::cw<"width">, &rectangle::width},
			js::struct_member{util::cw<"height">, &rectangle::height},
		};
};

using shape = std::variant<circle, rectangle>;

namespace js {
template <>
struct union_of<shape> {
		constexpr static auto& discriminant = util::cw<"type">;
		constexpr static auto alternatives = std::tuple{
			alternative<circle>{"circle"},
			alternative<rectangle>{"rectangle"},
		};
};
} // namespace js

// ── Class (object wrap) ───────────────────────────────────────────────────────
// A C++ object whose lifetime is managed by the JS garbage collector.
class canvas {
	public:
		// Marks this type for "object wrap" style transfer.
		using transfer_type = js::tagged_external<canvas>;

		explicit canvas(std::u8string name) :
				name_{std::move(name)},
				created_at_{js::js_clock::now()} {}

		// Instance methods. The environment& parameter is optional; omit it if unused.
		auto add(shape s) -> void { shapes_.push_back(std::move(s)); }
		auto count() const -> int32_t { return static_cast<int32_t>(shapes_.size()); }
		auto get_name() const -> std::u8string { return name_; }
		auto get_created_at() const -> js::js_clock::time_point { return created_at_; }

		// Static factory — called as Canvas.create(name) from JavaScript.
		static auto create(environment& env, std::u8string name) -> js::forward<js::napi::value<js::object_tag>> {
			return js::forward{canvas::class_template(env).construct(env, std::move(name))};
		}

		static auto class_template(environment& env) -> js::napi::value<js::class_tag_of<canvas>> {
			return env.class_template(
				std::type_identity<canvas>{},
				js::class_template{
					js::class_constructor{util::cw<"Canvas">},
					js::class_method{util::cw<"add">, util::fn<&canvas::add>},
					js::class_method{util::cw<"count">, util::fn<&canvas::count>},
					js::class_method{util::cw<"getName">, util::fn<&canvas::get_name>},
					js::class_method{util::cw<"getCreatedAt">, util::fn<&canvas::get_created_at>},
					js::class_static{util::cw<"create">, canvas::create},
				}
			);
		}

	private:
		std::u8string name_;
		js::js_clock::time_point created_at_;
		std::vector<shape> shapes_;
};

// ── Free functions ────────────────────────────────────────────────────────────

// Strings: ASCII (throws on non-ASCII input), UTF-8, UTF-16.
auto echo_ascii(std::string s) -> std::string {
	return s;
}
auto echo_utf8(std::u8string s) -> std::u8string {
	return s;
}
auto echo_utf16(std::u16string s) -> std::u16string {
	return s;
}

// Integrals: coerced from JS number; throws if out of range.
auto add_ints(int32_t a, int32_t b) -> int32_t {
	return a + b;
}
auto add_uints(uint32_t a, uint32_t b) -> uint32_t {
	return a + b;
}
auto negate_bool(bool v) -> bool {
	return !v;
}

// Bigints: int64_t / uint64_t accept JS bigint values.
auto bigint_add(int64_t a, int64_t b) -> int64_t {
	return a + b;
}

// Optional + enum: undefined from JS becomes std::nullopt.
auto describe_direction(std::optional<direction> d) -> std::u8string {
	if (!d)
		return u8"(none)";
	switch (*d) {
		case direction::north: return u8"heading north";
		case direction::south: return u8"heading south";
		case direction::east: return u8"heading east";
		case direction::west: return u8"heading west";
	}
}

// Structs: transferred to and from JavaScript by value.
auto midpoint(point a, point b) -> point {
	return {(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
}

auto mix_colors(color_rgb a, color_rgb b) -> color_rgb {
	return {(a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2};
}

// Arrays: std::vector<T> accepts a JavaScript array.
auto centroid(std::vector<point> pts) -> point {
	point sum{};
	for (auto& p : pts) {
		sum.x += p.x;
		sum.y += p.y;
	}
	auto n = static_cast<double>(pts.size());
	return {sum.x / n, sum.y / n};
}

// Discriminated union: JS passes { type: "circle", ... } or { type: "rectangle", ... }.
auto area(shape value) -> double {
	return std::visit(
		util::overloaded{
			[](circle& value) -> double {
				return 3.14159265358979 * value.radius * value.radius;
			},
			[](rectangle& value) -> double {
				return value.width * value.height;
			},
		},
		value
	);
}

// Variant: accepts either a string or a number; resolved from the JS value type.
auto describe_value(std::variant<std::u16string, double> value) -> std::u8string {
	return std::visit(
		util::overloaded{
			[](std::u16string& /*value*/) -> std::u8string { return u8"string"; },
			[](double& /*value*/) -> std::u8string { return u8"number"; }
		},
		value
	);
}

// Date: js::js_clock is std::chrono-compatible and losslessly represents a JS Date.
auto elapsed_ms(js::js_clock::time_point t) -> double {
	return std::chrono::duration<double, std::milli>(js::js_clock::now() - t).count();
}

// ── Module ────────────────────────────────────────────────────────────────────
js::napi::napi_js_module module_namespace{
	std::type_identity<environment>{},
	[](environment& env) -> auto {
		return std::tuple{
			// Strings
			std::pair{util::cw<"echoAscii">, js::free_function{echo_ascii}},
			std::pair{util::cw<"echoUtf8">, js::free_function{echo_utf8}},
			std::pair{util::cw<"echoUtf16">, js::free_function{echo_utf16}},

			// Integrals
			std::pair{util::cw<"addInts">, js::free_function{add_ints}},
			std::pair{util::cw<"addUints">, js::free_function{add_uints}},
			std::pair{util::cw<"negateBool">, js::free_function{negate_bool}},

			// Bigints
			std::pair{util::cw<"bigintAdd">, js::free_function{bigint_add}},

			// Optional + enum
			std::pair{util::cw<"describeDirection">, js::free_function{describe_direction}},

			// Structs
			std::pair{util::cw<"midpoint">, js::free_function{midpoint}},
			std::pair{util::cw<"mixColors">, js::free_function{mix_colors}},

			// Arrays
			std::pair{util::cw<"centroid">, js::free_function{centroid}},

			// Discriminated union
			std::pair{util::cw<"area">, js::free_function{area}},

			// Variant
			std::pair{util::cw<"describeValue">, js::free_function{describe_value}},

			// Date
			std::pair{util::cw<"elapsedMs">, js::free_function{elapsed_ms}},

			// Class (object wrap)
			std::pair{util::cw<"Canvas">, js::forward{canvas::class_template(env)}},
		};
	}
};
