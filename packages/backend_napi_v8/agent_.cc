module;
#include <type_traits>
export module backend_napi_v8:agent;
import :environment;
import :runtime;
import isolated_js;
import napi_js;

namespace backend_napi_v8 {

// This storage is managed by the external isolate, not napi.
export class agent_environment {
	public:
		explicit agent_environment(const isolated_v8::agent_lock& lock);
		auto runtime() -> auto& { return runtime_interface_; }

	private:
		runtime_interface runtime_interface_;
};

using agent_handle = isolated_v8::agent_handle<agent_environment>;

export auto agent_class_template(environment& env) -> js::napi::value<js::class_tag_of<agent_handle>>;

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::agent_handle;
template <>
struct transfer_type<agent_handle> : std::type_identity<js::tagged_external_of<agent_handle>> {};
} // namespace js
