module;
#include <cassert>
module napi_js;
import std;
import v8;

namespace js::napi {

// `environment_lock_witness`
auto environment_lock_witness::make_witness(napi::environment& env) -> environment_lock_witness {
	auto* isolate = env.isolate();
	if (isolate == nullptr) {
		return environment_lock_witness{env, nullptr, {}};
	} else {
		return environment_lock_witness{env, isolate, isolate->GetCurrentContext()};
	}
}

// `environment_try_catch`
environment_try_catch::environment_try_catch(environment_lock_witness lock) {
	if (auto* isolate = lock.isolate(); isolate != nullptr) {
		try_catch_.emplace(isolate);
	}
}

environment_try_catch::~environment_try_catch() {
	assert(!try_catch_ || !try_catch_->HasCaught());
}

auto environment_try_catch::take_exception() -> napi_value {
	if (try_catch_ && try_catch_->HasCaught()) {
		auto exception = try_catch_->Exception();
		try_catch_->Reset();
		return std::bit_cast<napi_value>(exception);
	} else {
		throw std::logic_error{"no pending v8 exception"};
	}
}

} // namespace js::napi
