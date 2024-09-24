module;
#include <tuple>
module ivm.isolated_v8;
import :agent;
import :realm;
import :script;
import ivm.iv8;
import ivm.value;
import v8;

namespace ivm {

// source_location
auto source_location::line() const -> int {
	return line_;
}

auto source_location::column() const -> int {
	return column_;
}

// source_origin
source_origin::source_origin(v8::Local<v8::String> resource_name, source_location location) :
		resource_name_{resource_name},
		location_{location} {
}

auto source_origin::location() const -> source_location {
	return location_;
}

auto source_origin::resource_name() const -> v8::Local<v8::String> {
	return resource_name_;
}

// script
script::script(agent::lock& agent, v8::Local<v8::UnboundScript> script) :
		unbound_script_{agent->isolate(), script} {
}

auto script::run(realm::scope& realm_scope) -> value::value_t {
	auto* isolate = realm_scope.isolate();
	auto script = v8::Local<v8::UnboundScript>::New(isolate, unbound_script_);
	auto context = realm_scope.context();
	auto result = script->BindToCurrentContext()->Run(context).ToLocalChecked();
	return value::transfer<value::value_t>(result, std::tuple{isolate, context}, std::tuple{});
}

auto script::compile(agent::lock& agent, v8::Local<v8::String> code_string, const source_origin& source_origin) -> script {
	v8::Context::Scope context_scope{agent->scratch_context()};
	v8::ScriptOrigin origin{
		source_origin.resource_name(),
		source_origin.location().line(),
		source_origin.location().column()
	};
	v8::ScriptCompiler::Source source{code_string, origin};
	auto* isolate = agent->isolate();
	auto script_handle = v8::ScriptCompiler::CompileUnboundScript(isolate, &source).ToLocalChecked();
	return script{agent, script_handle};
}

} // namespace ivm
