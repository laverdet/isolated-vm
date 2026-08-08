module v8_js;
import :evaluation.script;
import :unmaybe;
import auto_js;
import std;
import v8;

namespace js::iv8 {

auto script::compile(context_lock_witness lock, v8::Local<v8::String> code_string, source_origin source_origin) -> v8::MaybeLocal<script> {
	auto location = source_origin.location.value_or(source_location{});
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(std::move(source_origin).name, lock);
	v8::Local<v8::String> resource_name{};
	// nb: Empty handle is ok for `v8::ScriptOrigin`
	std::ignore = maybe_resource_name.ToLocal(&resource_name);
	v8::ScriptOrigin origin{resource_name, location.line, location.column};
	v8::ScriptCompiler::Source source{code_string, origin};
	auto unbound_script = v8::ScriptCompiler::CompileUnboundScript(lock.isolate(), &source);
	// nb: `As<script>()` doesn't work because `v8::UnboundScript` doesn't provide a `Cast` function
	return std::bit_cast<v8::MaybeLocal<script>>(unbound_script);
}

auto script::run(context_lock_witness lock) -> v8::Local<v8::Value> {
	auto allow_js = v8::Isolate::AllowJavascriptExecutionScope{lock.isolate()};
	return unmaybe(BindToCurrentContext()->Run(lock.context()));
}

} // namespace js::iv8
