module isolated_v8.function;
import v8;

namespace isolated_v8 {

function_template::function_template(agent::lock& agent, v8::Local<v8::FunctionTemplate> function) :
		function_{make_shared_remote(agent, function)} {}

auto function_template::make_function(realm::scope& realm) -> v8::Local<v8::Function> {
	auto function_template = function_->deref(realm.isolate());
	return function_template->GetFunction(realm.context());
}

} // namespace isolated_v8
