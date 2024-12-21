module;
#include <functional>
#include <optional>
#include <tuple>
#include <utility>
export module ivm.isolated_v8:script;
import :agent;
import :realm;
import ivm.iv8;
import ivm.utility;
import ivm.value;
import v8;

namespace ivm {

export struct source_location {
		int line{};
		int column{};
};

export struct source_origin {
		std::optional<value::string_t> name;
		std::optional<source_location> location;
};

export class script : util::non_copyable {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> value::value_t;
		static auto compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script;

		v8::Global<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script {
	auto local_code_string = value::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(code_string)>(code_string),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	return script::compile(agent, local_code_string, std::move(source_origin));
}

} // namespace ivm

namespace ivm::value {

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

} // namespace ivm::value
