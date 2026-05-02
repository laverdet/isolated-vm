export module backend_napi_v8:reference;
import :agent;
import :lock;
import auto_js;
import napi_js;
import std;
import v8_js;

namespace backend_napi_v8 {

export class reference_handle {
	public:
		using transfer_type = js::tagged_external<reference_handle>;

		explicit reference_handle(js::null_tag /*tag*/);
		explicit reference_handle(js::undefined_tag /*tag*/);
		reference_handle(agent_handle agent, js::typeof_kind type_of, js::iv8::shared_remote<v8::Context> realm, js::iv8::shared_remote<v8::Value> value);
		reference_handle(const agent_handle::lock& lock, agent_handle agent, js::iv8::shared_remote<v8::Context> realm, v8::Local<v8::Value> value);
		reference_handle(const agent_handle::lock& lock, agent_handle agent, js::iv8::shared_remote<v8::Context> realm, v8::Local<v8::Object> value);
		auto copy(environment& env) -> js::forward<js::napi::value_of<>>;
		auto get(environment& env, js::string_t name) -> js::forward<js::napi::value_of<>>;
		auto set(environment& env, js::string_t name, js::forward<js::napi::value_of<>> value_local) -> js::forward<js::napi::value_of<>>;
		auto invoke(environment& env, js::forward<js::napi::value_of<list_tag>> params_local) -> js::forward<js::napi::value_of<>>;

		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<reference_handle>>;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<v8::Context> realm_;
		js::iv8::shared_remote<v8::Value> value_;
		js::typeof_kind typeof_;
};

} // namespace backend_napi_v8
