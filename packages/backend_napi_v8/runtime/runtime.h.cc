export module backend_napi_v8:runtime;
import isolated_v8;
import v8_js;
namespace v8 = embedded_v8;

namespace backend_napi_v8 {

export class runtime_interface {
	public:
		explicit runtime_interface(const isolated_v8::agent_lock& agent);
		auto instantiate(const isolated_v8::realm_scope& realm) -> v8::Local<v8::Module>;

	private:
		js::iv8::unique_remote<v8::FunctionTemplate> clock_time_;
		js::iv8::unique_remote<v8::FunctionTemplate> performance_time_;
};

} // namespace backend_napi_v8
