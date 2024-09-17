module ivm.isolated_v8;
import :agent;
import :realm;
import :script;
import ivm.v8;
import ivm.value;
import v8;

namespace ivm {

script::script(v8::Isolate* isolate, v8::Local<v8::UnboundScript> script) :
		unbound_script_{isolate, script} {
}

auto script::run(realm::scope& realm_scope) -> value::value_t {
	auto* isolate = realm_scope.isolate();
	auto script = v8::Local<v8::UnboundScript>::New(isolate, unbound_script_);
	auto context = realm_scope.context();
	auto result = script->BindToCurrentContext()->Run(context).ToLocalChecked();
	iv8::handle<v8::Value> handle{result, iv8::handle_env{isolate, context}};
	return value::transfer<value::value_t>(handle);
}

auto script::compile(agent::lock& agent, v8::Local<v8::String> code_string) -> script {
	v8::Context::Scope context_scope{agent->scratch_context()};
	v8::ScriptCompiler::Source source{code_string};
	auto* isolate = agent->isolate();
	auto script_handle = v8::ScriptCompiler::CompileUnboundScript(isolate, &source).ToLocalChecked();
	return script{isolate, script_handle};
}

} // namespace ivm
