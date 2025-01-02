module;
#include <tuple>
module ivm.isolated_v8;
import :agent;
import :realm;
import :script;
import ivm.iv8;
import ivm.js;
import v8;

namespace ivm {

// script
script::script(agent::lock& agent, v8::Local<v8::UnboundScript> script) :
		unbound_script_{agent->isolate(), script} {
}

auto script::run(realm::scope& realm_scope) -> js::value_t {
	auto* isolate = realm_scope.isolate();
	auto script = unbound_script_.Get(isolate);
	auto context = realm_scope.context();
	auto result = script->BindToCurrentContext()->Run(context).ToLocalChecked();
	return js::transfer<js::value_t>(result, std::tuple{isolate, context}, std::tuple{});
}

auto script::compile(agent::lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> script {
	v8::Context::Scope context_scope{agent->scratch_context()};
	auto maybe_resource_name = js::transfer_strict<v8::MaybeLocal<v8::String>>(
		std::move(source_origin.name),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	v8::Local<v8::String> resource_name{};
	(void)maybe_resource_name.ToLocal(&resource_name);
	auto location = source_origin.location.value_or(source_location{});
	v8::ScriptOrigin origin{resource_name, location.line, location.column};
	v8::ScriptCompiler::Source source{code_string, origin};
	auto* isolate = agent->isolate();
	auto script_handle = v8::ScriptCompiler::CompileUnboundScript(isolate, &source).ToLocalChecked();
	return script{agent, script_handle};
}

} // namespace ivm
