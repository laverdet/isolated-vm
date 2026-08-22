export module backend_napi_v8:runtime;
import auto_js;
import std;
import v8_js;

namespace backend_napi_v8 {

// Opaque record returned by the runtime `transfer` function. It is unwrapped by `match` when the
// value leaves the isolate.
class transfer_record {
	private:
		using external_type = js::iv8::tagged_external<transfer_record>;

	public:
		transfer_record(v8::Isolate* isolate, v8::Local<v8::Value> subject, std::optional<js::iv8::value_of<js::list_tag>> transfer);

		// Wraps a value & its transfer list into an opaque external
		static auto make(
			const js::iv8::isolated::realm_scope& lock,
			v8::Local<v8::Value> subject,
			std::optional<js::iv8::value_of<js::list_tag>> transfer
		) -> v8::Local<v8::Value>;

		// Matches an opaque record, returning `nullptr` for any other value
		static auto match(v8::Local<v8::Value> value) -> transfer_record*;

		[[nodiscard]] auto subject(js::iv8::isolate_lock_witness lock) const -> v8::Local<v8::Value>;
		[[nodiscard]] auto transfer(js::iv8::context_lock_witness lock) const -> std::optional<js::iv8::value_of<js::list_tag>>;

	private:
		v8::Global<v8::Value> subject_;
		v8::Global<v8::Array> transfer_;
};

class runtime_interface {
	public:
		explicit runtime_interface(const js::iv8::isolated::agent_lock& lock);
		auto instantiate(js::iv8::context_lock_witness lock) -> v8::Local<js::iv8::module_record>;

	private:
		js::iv8::unique_remote<v8::FunctionTemplate> clock_time_;
		js::iv8::unique_remote<v8::FunctionTemplate> performance_time_;
		js::iv8::unique_remote<v8::FunctionTemplate> transfer_;
};

} // namespace backend_napi_v8
