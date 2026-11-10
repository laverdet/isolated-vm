module;
#include <cassert>
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
auto executor::exchange_timeout(timeout* timeout) noexcept -> void {
	if (current_timeout_ == timeout) {
		return;
	}
	auto* previous = std::exchange(current_timeout_, timeout);
	if (previous != nullptr) {
		previous->deactivate();
	}
	if (timeout != nullptr) {
		timeout->activate();
	}
}

executor::timeout::timeout(executor& executor, duration_type duration) :
		executor_{executor},
		timer_{std::defer_lock, duration},
		callback_{std::in_place, timer_.get_token(), expire{executor}} {}

executor::timeout::~timeout() {
	assert(executor_.get().current_timeout_ != this);
	callback_.reset();
}

auto executor::timeout::activate() noexcept -> void {
	if (timer_.stop_requested()) {
		executor_.get().isolate()->TerminateExecution();
	} else {
		timer_.resume();
	}
}

auto executor::timeout::deactivate() noexcept -> void {
	timer_.pause();
	auto* isolate = executor_.get().isolate();
	if (timer_.stop_requested() && !executor_.get().is_terminated() && !isolate->IsExecutionTerminating()) {
		// Withdraw unobserved termination request. Yes, the `IsExecutionTerminating()` check above is
		// needed because `CancelTerminateExecution` additionally clears the pending exception after the
		// interrupt request has been served.
		isolate->CancelTerminateExecution();
	}
}

auto executor::timeout::expire::operator()() const noexcept -> void {
	executor_->isolate()->RequestInterrupt(&interrupt, executor_);
}

auto executor::timeout::expire::interrupt(v8::Isolate* isolate, void* data) -> void {
	auto* current = static_cast<executor*>(data)->current_timeout_;
	if (current != nullptr && current->timer_.stop_requested()) {
		isolate->TerminateExecution();
	}
}

// executor::timeout::scope
executor::timeout::scope::scope(executor& executor, timeout* timeout) :
		executor_{executor},
		previous_{executor.current_timeout_} {
	executor.exchange_timeout(timeout);
}

executor::timeout::scope::~scope() {
	executor_.get().exchange_timeout(previous_);
}

} // namespace js::iv8::isolated
