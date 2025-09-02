export module backend_napi_v8:agent;
import :environment;
import :runtime;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

export class agent_environment {
	public:
		explicit agent_environment(const isolated_v8::agent_lock& lock);
		auto runtime() -> auto& { return runtime_interface_; }

	private:
		runtime_interface runtime_interface_;
};

using agent_handle = isolated_v8::agent_handle<agent_environment>;

export auto make_create_agent(environment& env) -> js::napi::value<js::function_tag>;

} // namespace backend_napi_v8
