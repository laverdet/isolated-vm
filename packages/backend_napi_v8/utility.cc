module backend_napi_v8;
import :utility;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {
namespace {
thread_local util::timer_stop_token* current_execution_timeout{};
}

// `execution_timeout`
execution_timeout::execution_timeout(js::iv8::isolated::executor& executor, js::js_clock::duration duration) :
		executor_{executor},
		timer_{duration},
		scope_{current_execution_timeout, &timer_},
		callback_{std::in_place, timer_.get_token(), terminate{executor.isolate()}} {}

execution_timeout::~execution_timeout() {
	callback_.reset();
	if (timer_.stop_requested() && !executor_.get().is_terminated()) {
		// Prevent a completed but timed out script from terminating the next script
		executor_.get().isolate()->CancelTerminateExecution();
	}
}

auto execution_timeout::terminate::operator()() const noexcept -> void {
	isolate_->TerminateExecution();
}

// `execution_timeout_suspension`
execution_timeout_suspension::execution_timeout_suspension() :
		timer_{current_execution_timeout} {
	if (timer_ != nullptr) {
		timer_->pause();
	}
}

execution_timeout_suspension::~execution_timeout_suspension() {
	if (timer_ != nullptr) {
		timer_->resume();
	}
}

} // namespace backend_napi_v8
