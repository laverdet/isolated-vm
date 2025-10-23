module;
#include <expected>
#include <utility>
module isolated_v8;
import isolated_js;
import :agent_host;
import :realm;
import :remote;
import :script;
import v8_js;
import v8;

namespace isolated_v8 {

auto run_script(const realm::scope& realm, script_local_type script) -> std::expected<js::value_t, js::error_value> {
	auto result = script->BindToCurrentContext()->Run(realm.context()).ToLocalChecked();
	return js::transfer_out<js::value_t>(result, realm);
}

auto compile_script_direct(const agent_lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> std::expected<script_local_type, js::error_value> {
	// nb: It is undocumented (and even mentions "context independent"), but the script compiler
	// actually needs a context because it can throw an error and *that* would need a context.
	js::iv8::context_managed_lock context{agent, agent->scratch_context()};
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(std::move(source_origin.name), agent);
	v8::Local<v8::String> resource_name{};
	(void)maybe_resource_name.ToLocal(&resource_name);
	auto location = source_origin.location.value_or(source_location{});
	v8::ScriptOrigin origin{resource_name, location.line, location.column};
	v8::ScriptCompiler::Source source{code_string, origin};
	auto* isolate = agent->isolate();
	return js::iv8::unmaybe_one(agent, [ & ] -> v8::MaybeLocal<v8::UnboundScript> {
		return v8::ScriptCompiler::CompileUnboundScript(isolate, &source);
	});
}

} // namespace isolated_v8
