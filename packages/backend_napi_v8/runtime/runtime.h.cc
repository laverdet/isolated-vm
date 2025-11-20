export module backend_napi_v8:runtime;
import v8_js;

namespace backend_napi_v8 {

export class runtime_interface {
	public:
		explicit runtime_interface(const js::iv8::isolated::agent_lock& lock);
		auto instantiate(js::iv8::context_lock_witness lock) -> v8::Local<v8::Module>;

	private:
		js::iv8::unique_remote<v8::FunctionTemplate> clock_time_;
		js::iv8::unique_remote<v8::FunctionTemplate> performance_time_;
};

} // namespace backend_napi_v8
