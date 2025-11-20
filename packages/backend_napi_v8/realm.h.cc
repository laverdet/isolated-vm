export module backend_napi_v8:realm;
import :agent;
import :environment;
import isolated_js;
import napi_js;
import v8_js;

namespace backend_napi_v8 {

export class realm_handle {
	public:
		using transfer_type = js::tagged_external<realm_handle>;

		realm_handle(agent_handle agent, js::iv8::shared_remote<v8::Context> realm);

		auto agent() -> auto& { return agent_; }
		auto realm() -> auto& { return realm_; }

		auto instantiate_runtime(environment& env) -> js::napi::value<js::promise_tag>;

		static auto create(agent_handle& agent, environment& env) -> js::napi::value<js::promise_tag>;

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<realm_handle>>;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<v8::Context> realm_;
};

} // namespace backend_napi_v8
