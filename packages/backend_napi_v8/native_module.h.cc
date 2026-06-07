export module backend_napi_v8:native_module;
import :realm;
import auto_js;
import isolated_vm;
import napi_js;
import nodejs;
import std;
import util;

namespace backend_napi_v8 {

struct create_native_module_options {
		std::u16string origin;
		std::optional<std::string> suffix;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &create_native_module_options::origin},
			js::struct_member{util::cw<"suffix">, &create_native_module_options::suffix},
		};
};

export class native_module_handle {
	public:
		explicit native_module_handle(
			js::napi::uv_dlib lib,
			isolated_vm::detail::initialize_addon* initialize,
			create_native_module_options options,
			std::vector<std::u16string> names
		);

		auto instantiate(environment& env, realm_handle& realm) -> js::forward<js::napi::value_of<>>;
		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<native_module_handle>>;
		static auto create(environment& env, std::string filename, create_native_module_options options) -> js::forward<js::napi::value_of<>>;

	private:
		js::napi::uv_dlib lib_;
		isolated_vm::detail::initialize_addon* initialize_;
		create_native_module_options options_;
		std::vector<std::u16string> names_;
};

} // namespace backend_napi_v8
