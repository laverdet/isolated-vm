export module backend_napi_v8.script;
import backend_napi_v8.environment;
import isolated_js;
import ivm.napi;

namespace backend_napi_v8 {

export auto make_compile_script(environment& env) -> js::napi::value<js::function_tag>;
export auto make_run_script(environment& env) -> js::napi::value<js::function_tag>;

} // namespace backend_napi_v8
