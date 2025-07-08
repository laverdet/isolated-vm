module;
#include <tuple>
module isolated_v8;
import isolated_js;
import :agent;
import :realm;
import :remote;
import v8_js;
import v8;

namespace isolated_v8 {

// script
script::script(agent::lock& agent, v8::Local<v8::UnboundScript> script) :
		unbound_script_{make_shared_remote(agent, script)} {
}

auto script::run(realm::scope& realm) -> js::value_t {
	auto script = unbound_script_->deref(realm);
	auto result = script->BindToCurrentContext()->Run(realm.context()).ToLocalChecked();
	return js::transfer_out<js::value_t>(result, realm);
}

auto script::compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script {
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
	auto script_handle = v8::ScriptCompiler::CompileUnboundScript(isolate, &source).ToLocalChecked();
	return script{agent, script_handle};
}

} // namespace isolated_v8
