module v8_js;
import :isolated.agent;
import :isolated.executor;
import std;

namespace js::iv8::isolated {

auto executor::reset() -> void {
	isolate_.reset();
}

auto executor::terminate() -> void {
	terminated_.store(true, std::memory_order_release);
	isolate_->TerminateExecution();
}

auto executor::is_terminated() const -> bool {
	return terminated_.load(std::memory_order_acquire);
}

auto executor::on_before_call_entered(v8::Isolate* isolate) -> void {
	// Checks for termination right after control has been passed to v8. This catches the case where a
	// task is slow to start and `TerminateExecution()`, from `terminate()` is invoked on an idle
	// isolate. In that case the termination is "handled" already.
	auto& self = agent_host::get_current(isolate).executor();
	if (self.is_terminated()) {
		isolate->TerminateExecution();
	}
}

} // namespace js::iv8::isolated
