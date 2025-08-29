export module backend_napi_v8:module_;
import :environment;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

export class module_handle {
	public:
		module_handle(isolated_v8::agent_handle agent, isolated_v8::js_module module);

		auto agent() -> auto& { return agent_; }
		auto module() -> auto& { return module_; }

		static auto make_compile_module(environment& env) -> js::napi::value<js::function_tag>;
		static auto make_create_capability(environment& env) -> js::napi::value<js::function_tag>;
		static auto make_evaluate_module(environment& env) -> js::napi::value<js::function_tag>;
		static auto make_link_module(environment& env) -> js::napi::value<js::function_tag>;

	private:
		isolated_v8::agent_handle agent_;
		isolated_v8::js_module module_;
};

} // namespace backend_napi_v8
