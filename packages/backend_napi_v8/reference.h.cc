export module backend_napi_v8:reference;
import :agent_handle;
import :environment;
import :utility;
import auto_js;
import napi_js;
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
		auto copy(const environment::lock& lock) -> forward_promise_type;
		auto get(const environment::lock& lock, js::string_t name) -> forward_promise_type;
		auto set(const environment::lock& lock, js::string_t name, js::forward<js::napi::local_of<>> value_local) -> forward_promise_type;
		auto invoke(const environment::lock& lock, js::forward<js::napi::local_of<list_tag>> params_local, transfer_options options) -> forward_promise_type;

		static auto class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<reference_handle>>;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<v8::Context> realm_;
		js::iv8::shared_remote<v8::Value> value_;
		js::typeof_kind typeof_;
};

} // namespace backend_napi_v8
