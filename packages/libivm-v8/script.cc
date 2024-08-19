export module ivm.isolated_v8:script;
import :agent;
import :realm;
import ivm.v8;
import ivm.value;
import v8;

namespace ivm {

export class script : non_copyable {
	public:
		script() = delete;
		script(v8::Isolate* isolate, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> value::value_t;
		static auto compile(agent::lock& lock, auto&& code_string) -> script;

	private:
		v8::Global<v8::UnboundScript> unbound_script_;
};

script::script(v8::Isolate* isolate, v8::Local<v8::UnboundScript> script) :
		unbound_script_{isolate, script} {
}

auto script::run(realm::scope& realm_scope) -> value::value_t {
	auto* isolate = realm_scope.isolate();
	auto script = v8::Local<v8::UnboundScript>::New(isolate, unbound_script_);
	auto context = realm_scope.context();
	auto result = iv8::unmaybe(script->BindToCurrentContext()->Run(context));
	iv8::handle<v8::Value> handle{result, iv8::handle_env{isolate, context}};
	return value::transfer<value::value_t>(handle);
}

auto script::compile(agent::lock& lock, auto&& code_string) -> script {
	auto local_string = value::transfer_strict<v8::Local<v8::String>>(code_string, lock->isolate());
	v8::Context::Scope context_scope{lock->scratch_context()};
	v8::ScriptCompiler::Source source{local_string};
	auto* isolate = lock->isolate();
	auto script_handle = iv8::unmaybe((v8::ScriptCompiler::CompileUnboundScript(isolate, &source)));
	return script{isolate, script_handle};
}

} // namespace ivm
