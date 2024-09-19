module;
#include <utility>
export module ivm.isolated_v8:script;
import :agent;
import :realm;
import ivm.utility;
import ivm.v8;
import ivm.value;
import v8;

namespace ivm {

export class script : util::non_copyable {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> value::value_t;
		static auto compile(agent::lock& agent, auto&& code_string) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string) -> script;

		v8::Global<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& agent, auto&& code_string) -> script {
	auto local_code_string = value::transfer_strict<v8::Local<v8::String>>(std::forward<decltype(code_string)>(code_string), agent->isolate());
	return script::compile(agent, local_code_string);
}

} // namespace ivm
