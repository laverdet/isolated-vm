module v8_js;
import :evaluation.script;
import :unmaybe;
import auto_js;
import std;
import v8;

namespace js::iv8 {

auto script::compile(context_lock_witness lock, v8::Local<v8::String> code_string, source_origin source_origin) -> expected_script_type {
	auto location = source_origin.location.value_or(source_location{});
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(std::move(source_origin).name, lock);
	v8::Local<v8::String> resource_name{};
	// nb: Empty handle is ok for `v8::ScriptOrigin`
	std::ignore = maybe_resource_name.ToLocal(&resource_name);
	v8::ScriptOrigin origin{resource_name, location.line, location.column};
	v8::ScriptCompiler::Source source{code_string, origin};
	return unmaybe_one(lock, [ & ] -> v8::MaybeLocal<v8::UnboundScript> {
		return v8::ScriptCompiler::CompileUnboundScript(lock.isolate(), &source);
	});
}

auto script::run(context_lock_witness lock, v8::Local<v8::UnboundScript> script) -> expected_value_type {
	return invoke_externalized_error_scope(lock, [ & ]() -> js::value_t {
		auto maybe_result = script->BindToCurrentContext()->Run(lock.context());
		return js::transfer_out<js::value_t>(unmaybe(maybe_result), lock);
	});
}

} // namespace js::iv8
