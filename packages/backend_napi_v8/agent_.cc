export module backend_napi_v8:agent;
import :environment;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

export class agent_handle {
	public:
		explicit agent_handle(const isolated_v8::agent_lock& lock, isolated_v8::agent_handle agent);

		auto agent() -> auto& { return agent_; }

		static auto make_create_agent(environment& env) -> js::napi::value<js::function_tag>;

	private:
		isolated_v8::agent_handle agent_;
};

} // namespace backend_napi_v8
