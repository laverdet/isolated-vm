module napi_js;
import std;

namespace js::napi {

// napi_scheduler
napi_scheduler::napi_scheduler(napi_env env) :
		fn_{env, nullptr} {
	// It starts ref'd, so we need to unref it
	fn_.unref(env);
}

napi_scheduler::operator bool() const {
	return bool{fn_};
}

auto napi_scheduler::close(threadsafe_function_type::close_callback close) -> void {
	fn_.close(close);
}

auto napi_scheduler::decrement_ref(node_api_basic_env env) const -> void {
	auto& storage = *fn_;
	if (--storage.refs == 0) {
		fn_.unref(env);
	}
}

auto napi_scheduler::increment_ref(node_api_basic_env env) const -> void {
	auto& storage = *fn_;
	if (++storage.refs == 1) {
		fn_.ref(env);
	}
}

auto napi_scheduler::storage::operator()(napi_env env, napi_value value, task_type& task) -> void {
	task(env, value);
}

} // namespace js::napi
