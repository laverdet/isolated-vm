export module backend_napi_v8:module_;
import :environment;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

export auto make_compile_module(environment& env) -> js::napi::value<js::function_tag>;
export auto make_create_capability(environment& env) -> js::napi::value<js::function_tag>;
export auto make_evaluate_module(environment& env) -> js::napi::value<js::function_tag>;
export auto make_link_module(environment& env) -> js::napi::value<js::function_tag>;

} // namespace backend_napi_v8
