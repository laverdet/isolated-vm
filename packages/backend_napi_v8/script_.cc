module;
#include <utility>
export module backend_napi_v8:script;
import :environment;
import isolated_js;
import napi_js;
namespace v8 = embedded_v8;

namespace backend_napi_v8 {

export class script_handle {
	public:
		using script_type = isolated_v8::shared_remote<v8::UnboundScript>;

		explicit script_handle(script_type script) :
				script_{std::move(script)} {}

		[[nodiscard]] auto script() const -> auto& { return script_; }

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<script_handle>>;

	private:
		script_type script_;
};

export auto make_compile_script(environment& env) -> js::napi::value<js::function_tag>;

} // namespace backend_napi_v8
