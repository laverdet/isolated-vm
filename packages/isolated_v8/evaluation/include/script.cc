module;
#include <tuple>
#include <utility>
export module isolated_v8:script;
import :agent;
import :evaluation_origin;
import :realm;
import :remote;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export class script {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm) -> js::value_t;
		static auto compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script;

		shared_remote<v8::UnboundScript> unbound_script_;
};

auto script::compile(agent::lock& agent, auto&& code_string, source_origin source_origin) -> script {
	auto local_code_string = js::transfer_in_strict<v8::Local<v8::String>>(std::forward<decltype(code_string)>(code_string), agent);
	return script::compile(agent, local_code_string, std::move(source_origin));
}

} // namespace isolated_v8
