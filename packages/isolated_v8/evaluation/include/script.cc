module;
#include <tuple>
#include <utility>
export module isolated_v8.script;
import isolated_v8.agent;
import isolated_v8.evaluation.origin;
import isolated_v8.realm;
import isolated_v8.remote;
import ivm.iv8;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export class script {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> js::value_t;
		static auto compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script;

		shared_remote<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script {
	auto local_code_string = js::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(code_string)>(code_string),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	return script::compile(agent, local_code_string, std::move(source_origin));
}

} // namespace isolated_v8
