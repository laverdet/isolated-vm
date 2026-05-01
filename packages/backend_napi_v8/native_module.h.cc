export module backend_napi_v8:native_module;
import :environment;
import :realm;
import auto_js;
import isolated_vm;
import napi_js;
import nodejs;
import std;

namespace backend_napi_v8 {

export class native_module_handle {
	public:
		explicit native_module_handle(
			js::napi::uv_dlib lib,
			isolated_vm::detail::initialize_addon* initialize,
			std::u16string origin,
			std::vector<std::u16string> names
		);

		auto instantiate(environment& env, realm_handle& realm) -> js::forward<js::napi::value_of<>>;
		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<native_module_handle>>;
		static auto create(environment& env, std::string filename, std::u16string origin) -> js::forward<js::napi::value_of<>>;

	private:
		js::napi::uv_dlib lib_;
		isolated_vm::detail::initialize_addon* initialize_;
		std::u16string origin_;
		std::vector<std::u16string> names_;
};

} // namespace backend_napi_v8
