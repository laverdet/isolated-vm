module isolated_v8;
import :remote;
import v8_js;
import v8;

namespace isolated_v8 {

function_template::function_template(const agent::lock& agent, v8::Local<v8::FunctionTemplate> function) :
		function_{make_shared_remote(agent, function)} {}

auto function_template::make_function(const js::iv8::context_lock_witness& lock) -> v8::Local<v8::Function> {
	auto function_template = function_->deref(lock);
	return function_template->GetFunction(lock.context()).ToLocalChecked();
}

} // namespace isolated_v8
