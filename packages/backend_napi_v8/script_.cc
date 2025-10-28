module;
#include <optional>
#include <utility>
export module backend_napi_v8:script;
import :agent;
import :environment;
import :realm;
import isolated_js;
import napi_js;
namespace v8 = embedded_v8;

namespace backend_napi_v8 {

struct compile_script_options {
		std::optional<isolated_v8::source_origin> origin;
};

struct run_script_options {
		std::optional<double> timeout;
};

export class script_handle {
	public:
		using transfer_type = js::tagged_external_of<script_handle>;
		using script_type = isolated_v8::shared_remote<v8::UnboundScript>;

		explicit script_handle(script_type script) :
				script_{std::move(script)} {}

		auto run(environment& env, realm_handle& realm, run_script_options options) -> js::napi::value<promise_tag>;

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<script_handle>>;
		static auto compile_script(agent_handle& agent, environment& env, js::string_t code_string, compile_script_options options) -> js::napi::value<promise_tag>;

	private:
		script_type script_;
};

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::compile_script_options;
using backend_napi_v8::run_script_options;

template <>
struct struct_properties<compile_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{util::cw<"origin">, struct_member{&compile_script_options::origin}},
		};
};

template <>
struct struct_properties<run_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{util::cw<"timeout">, struct_member{&run_script_options::timeout}},
		};
};
} // namespace js
