module;
#include <cstring>
#include <functional>
module ivm.node;
import :environment;
import ivm.isolated_v8;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm {

environment::environment(Napi::Env env) :
		env_{env},
		isolate_{v8::Isolate::GetCurrent()} {
}

auto environment::collection() -> util::collection_group& {
	return collection_group_;
}

auto environment::cluster() -> ivm::cluster& {
	return cluster_;
}

auto environment::context() -> v8::Local<v8::Context> {
	if (context_ == v8::Local<v8::Context>{}) {
		context_ = isolate_->GetCurrentContext();
	}
	return context_;
}

auto environment::napi_env() -> Napi::Env {
	return env_;
}

auto environment::isolate() -> v8::Isolate* {
	return isolate_;
}

auto environment::get(Napi::Env env) -> environment& {
	return *env.GetInstanceData<environment>();
}

auto environment::with_context_local() -> util::scope_exit<util::defaulter_finalizer<v8::Local<v8::Context>>> {
	return util::scope_exit{util::defaulter_finalizer<v8::Local<v8::Context>>{context_}};
}

} // namespace ivm
