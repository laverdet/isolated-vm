module napi_js;
import std;

namespace js::napi {

// napi_scheduler
napi_scheduler::napi_scheduler(napi_env env) :
		fn_{env, nullptr} {
	// It starts ref'd, so we need to unref it
	fn_.unref(env);
}

napi_scheduler::operator bool() const noexcept {
	return bool{fn_};
}

auto napi_scheduler::close(threadsafe_function_type::close_callback close) noexcept -> void {
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

auto napi_scheduler::make_ref(node_api_basic_env env) const -> ref {
	return ref{env, *this};
}

auto napi_scheduler::storage::operator()(napi_env env, napi_value value, task_type& task) noexcept -> void {
	task(env, value);
}

// napi_scheduler::ref
napi_scheduler::ref::ref(node_api_basic_env env, napi_scheduler scheduler) :
		scheduler_{std::move(scheduler)} {
	scheduler_.increment_ref(env);
}

napi_scheduler::ref::~ref() {
	if (scheduler_) {
		scheduler_(
			[](napi_env env, napi_value /*nothing*/, const napi_scheduler& scheduler) noexcept -> void {
				scheduler.decrement_ref(env);
			},
			scheduler_
		);
	}
}

} // namespace js::napi
