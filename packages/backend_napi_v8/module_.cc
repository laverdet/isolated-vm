module;
#include <optional>
export module backend_napi_v8:module_;
import :agent;
import :environment;
import :realm;
import isolated_js;
import isolated_v8;
import napi_js;

namespace backend_napi_v8 {
using namespace isolated_v8;

struct compile_module_options {
		std::optional<source_origin> origin;

		compile_module_options() = default;
		explicit compile_module_options(std::nullopt_t /*default*/) noexcept {}
		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &compile_module_options::origin},
		};
};

struct create_capability_options {
		source_required_name origin;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &create_capability_options::origin},
		};
};

export class module_handle {
	public:
		using callback_type = js::forward<js::napi::value<js::function_tag>>;
		using transfer_type = js::tagged_external_of<module_handle>;
		module_handle(agent_handle agent, isolated_v8::js_module module);

		auto agent() -> auto& { return agent_; }

		auto evaluate(environment& env, realm_handle& realm) -> js::napi::value<promise_tag>;
		auto link(environment& env, realm_handle& realm, callback_type link_callback) -> js::napi::value<promise_tag>;

		static auto compile(agent_handle& agent, environment& env, js::string_t source_text, compile_module_options options) -> js::napi::value<promise_tag>;
		static auto create_capability(agent_handle& agent, environment& env, callback_type capability, create_capability_options options) -> js::napi::value<promise_tag>;

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<module_handle>>;

	private:
		agent_handle agent_;
		isolated_v8::js_module module_;
};

} // namespace backend_napi_v8
