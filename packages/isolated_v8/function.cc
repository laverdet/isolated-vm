module isolated_v8.function;
import v8_js;
import v8;

namespace isolated_v8 {

function_template::function_template(agent::lock& agent, v8::Local<v8::FunctionTemplate> function) :
		function_{make_shared_remote(agent, function)} {}

auto function_template::make_function(v8::Local<v8::Context> context) -> v8::Local<v8::Function> {
	js::iv8::isolate_managed_lock lock{context->GetIsolate()};
	auto function_template = function_->deref(lock);
	return function_template->GetFunction(context).ToLocalChecked();
}

} // namespace isolated_v8
