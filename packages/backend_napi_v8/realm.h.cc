export module backend_napi_v8:realm;
import :agent;
import :environment;
import isolated_js;
import isolated_v8;
import napi_js;

namespace backend_napi_v8 {

export class realm_handle {
	public:
		using transfer_type = js::tagged_external<realm_handle>;

		realm_handle(agent_handle agent, isolated_v8::realm realm);

		auto agent() -> auto& { return agent_; }
		auto realm() -> auto& { return realm_; }

		auto instantiate_runtime(environment& env) -> js::napi::value<js::promise_tag>;

		static auto create(agent_handle& agent, environment& env) -> js::napi::value<js::promise_tag>;

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<realm_handle>>;

	private:
		agent_handle agent_;
		isolated_v8::realm realm_;
};

} // namespace backend_napi_v8
