module v8_js;
import :isolated.agent;
import :isolated.executor;
import auto_js;
import std;
import util;

namespace js::iv8::isolated {

// executor
auto executor::reset() -> void {
	isolate_.reset();
}

auto executor::terminate() -> void {
	stop_source_.request_stop();
	isolate_->TerminateExecution();
}

auto executor::is_terminated() const -> bool {
	return stop_source_.stop_requested();
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

// executor::unlock
executor::unlock::unlock(isolate_lock_witness lock, executor& executor) :
		unlock_{lock},
		executor_{executor} {}

executor::unlock::~unlock() {
	if (executor_.get().is_terminated()) {
		executor_.get().isolate()->TerminateExecution();
	}
};

// executor::timeout
executor::timeout::timeout(executor& executor, js_clock::duration duration) :
		executor_{executor},
		timer_{duration},
		scope_{executor.current_timeout_, this},
		callback_{std::in_place, timer_.get_token(), expire{executor.isolate()}} {}

executor::timeout::~timeout() {
	callback_.reset();
	if (timer_.stop_requested() && !executor_.get().is_terminated()) {
		// Prevent a completed but timed out task from terminating the next one
		executor_.get().isolate()->CancelTerminateExecution();
	}
}

auto executor::timeout::expire::operator()() const noexcept -> void {
	isolate_->TerminateExecution();
}

// executor::timeout_suspension
executor::timeout_suspension::timeout_suspension(executor& executor) :
		timeout_{executor.current_timeout_} {
	if (timeout_ != nullptr) {
		timeout_->pause();
	}
}

executor::timeout_suspension::~timeout_suspension() {
	if (timeout_ != nullptr) {
		timeout_->resume();
	}
}

} // namespace js::iv8::isolated
