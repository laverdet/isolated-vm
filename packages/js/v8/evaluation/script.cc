module;
#include <expected>
#include <utility>
module v8_js;
import :evaluation.script;
import :unmaybe;
import isolated_js;
import v8;

namespace js::iv8 {

auto script::compile(context_lock_witness lock, v8::Local<v8::String> code_string, source_origin source_origin) -> expected_script_type {
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(std::move(source_origin).name, lock);
	v8::Local<v8::String> resource_name{};
	// nb: Empty handle is ok for `v8::ScriptOrigin`
	(void)maybe_resource_name.ToLocal(&resource_name);
	auto location = source_origin.location.value_or(source_location{});
	v8::ScriptOrigin origin{resource_name, location.line, location.column};
	v8::ScriptCompiler::Source source{code_string, origin};
	return unmaybe_one(lock, [ & ] -> v8::MaybeLocal<v8::UnboundScript> {
		return v8::ScriptCompiler::CompileUnboundScript(lock.isolate(), &source);
	});
}

auto script::run(context_lock_witness lock, v8::Local<v8::UnboundScript> script) -> expected_value_type {
	auto result = unmaybe_one(lock, [ & ] -> v8::MaybeLocal<v8::Value> {
		return script->BindToCurrentContext()->Run(lock.context());
	});
	if (result) {
		try {
			return expected_value_type{std::in_place, js::transfer_out<js::value_t>(*result, lock)};
		} catch (const js::error_value& error) {
			return expected_value_type{std::unexpect, js::error_value{error}};
		}
	} else {
		return expected_value_type{std::unexpect, std::move(result).error()};
	}
}

} // namespace js::iv8
