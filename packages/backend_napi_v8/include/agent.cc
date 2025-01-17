export module backend_napi_v8.agent;
import backend_napi_v8.environment;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

export auto make_create_agent(environment& env) -> js::napi::value<js::function_tag>;
export auto make_create_realm(environment& env) -> js::napi::value<js::function_tag>;

} // namespace backend_napi_v8
