export module backend_napi_v8:agent_handle;
import :runtime;
import v8_js;

namespace backend_napi_v8 {

// This storage is managed by the external isolate, not napi.
class agent_environment {
	public:
		explicit agent_environment(const js::iv8::isolated::agent_lock& lock) :
				runtime_interface_{lock} {}

		auto runtime() -> auto& { return runtime_interface_; }

	private:
		runtime_interface runtime_interface_;
};

// Internal handle which eventually holds a weak reference to the isolate
using agent_handle = js::iv8::isolated::agent_handle_of<agent_environment>;

} // namespace backend_napi_v8
