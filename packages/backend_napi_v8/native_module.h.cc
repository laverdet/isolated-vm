export module backend_napi_v8:native_module;
import :environment;
import auto_js;
import napi_js;
import nodejs;
import std;

namespace backend_napi_v8 {

export class native_module_handle {
	private:
		using initialize_signature_t = auto(v8::Isolate*, v8::Local<v8::Context>, v8::Local<v8::Object>) -> void;

	public:
		explicit native_module_handle(const std::string& filename);

		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<native_module_handle>>;
		static auto create(environment& env, std::string filename) -> js::forward<js::napi::value_of<>>;

	private:
		js::napi::uv_dlib lib_;
};

} // namespace backend_napi_v8
