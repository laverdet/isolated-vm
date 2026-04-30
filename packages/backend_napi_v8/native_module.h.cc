export module backend_napi_v8:native_module;
import :agent;
import :environment;
import auto_js;
import isolated_vm;
import napi_js;
import nodejs;
import std;

namespace backend_napi_v8 {

export class native_module_handle {
	public:
		explicit native_module_handle(js::napi::uv_dlib lib, isolated_vm::detail::registration_signature* registration, void* data);

		auto instantiate(environment& env, agent_handle& agent) -> js::forward<js::napi::value_of<>>;
		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<native_module_handle>>;
		static auto create(environment& env, std::string filename) -> js::forward<js::napi::value_of<>>;

	private:
		js::napi::uv_dlib lib_;
		isolated_vm::detail::registration_signature* registration_;
		void* registration_data_;
};

} // namespace backend_napi_v8
