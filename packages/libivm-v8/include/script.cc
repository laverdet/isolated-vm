export module ivm.isolated_v8:script;
import :agent;
import :agent.lock;
import :realm;
import ivm.utility;
import ivm.v8;
import ivm.value;
import v8;

namespace ivm {

export class script : util::non_copyable {
	public:
		script() = delete;
		script(v8::Isolate* isolate, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> value::value_t;
		static auto compile(agent::lock& lock, auto&& code_string) -> script;

	private:
		static auto compile(agent::lock& lock, v8::Local<v8::String> code_string) -> script;

		v8::Global<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& lock, auto&& code_string) -> script {
	auto local_string = value::transfer_strict<v8::Local<v8::String>>(code_string, lock->isolate());
	return script::compile(lock, local_string);
}

} // namespace ivm
