module;
#include <optional>
#include <tuple>
#include <utility>
export module ivm.isolated_v8:script;
import :agent;
import :realm;
import :remote;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export struct source_location {
		int line{};
		int column{};
};

export struct source_origin {
		std::optional<js::string_t> name;
		std::optional<source_location> location;
};

export class script {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> js::value_t;
		static auto compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script;

		shared_remote<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script {
	auto local_code_string = js::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(code_string)>(code_string),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	return script::compile(agent, local_code_string, std::move(source_origin));
}

} // namespace isolated_v8

namespace js {

using isolated_v8::source_location;
using isolated_v8::source_origin;

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

} // namespace js
