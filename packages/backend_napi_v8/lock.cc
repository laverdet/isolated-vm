export module backend_napi_v8:lock;
import v8_js;

namespace backend_napi_v8 {

export class agent_environment;
using agent_lock = js::iv8::isolated::agent_lock_of<agent_environment>;
using realm_scope = js::iv8::isolated::realm_scope_of<agent_environment>;

} // namespace backend_napi_v8
