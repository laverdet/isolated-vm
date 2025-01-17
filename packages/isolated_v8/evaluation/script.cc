module;
#include <tuple>
import isolated_v8.agent;
import isolated_v8.lock;
import isolated_v8.realm;
import isolated_v8.remote;
module isolated_v8.script;
import ivm.iv8;
import isolated_js;
import v8;

namespace isolated_v8 {

// script
script::script(agent::lock& agent, v8::Local<v8::UnboundScript> script) :
		unbound_script_{make_shared_remote(agent, script)} {
}

auto script::run(realm::scope& realm_scope) -> js::value_t {
	auto* isolate = realm_scope.isolate();
	auto script = unbound_script_->deref(realm_scope.agent());
	auto context = realm_scope.context();
	auto result = script->BindToCurrentContext()->Run(context).ToLocalChecked();
	return js::transfer_out<js::value_t>(result, isolate, context);
}

auto script::compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script {
	context_scope context{agent->scratch_context()};
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(std::move(source_origin.name), agent->isolate());
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
