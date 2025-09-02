export module backend_napi_v8:runtime;
import isolated_v8;
import v8_js;

namespace backend_napi_v8 {

export class runtime_interface {
	public:
		explicit runtime_interface(const isolated_v8::agent_lock& agent);
		auto instantiate(const isolated_v8::realm::scope& realm) -> isolated_v8::js_module;

	private:
		isolated_v8::function_template clock_time_;
		isolated_v8::function_template performance_time_;
};

} // namespace backend_napi_v8
