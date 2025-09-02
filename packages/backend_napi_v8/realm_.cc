export module backend_napi_v8:realm;
import :agent;
import :environment;
import isolated_js;
import isolated_v8;
import napi_js;

namespace backend_napi_v8 {

export class realm_handle {
	public:
		realm_handle(agent_handle agent, isolated_v8::realm realm);

		auto agent() -> auto& { return agent_; }
		auto realm() -> auto& { return realm_; }

		static auto make_create_realm(environment& env) -> js::napi::value<js::function_tag>;
		static auto make_instantiate_runtime(environment& env) -> js::napi::value<js::function_tag>;

	private:
		agent_handle agent_;
		isolated_v8::realm realm_;
};

} // namespace backend_napi_v8
