module;
#include <cstring>
#include <functional>
module ivm.node;
import :environment;
import ivm.isolated_v8;
import ivm.napi;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm {

environment::environment(napi_env env) :
		env_{env},
		isolate_{v8::Isolate::GetCurrent()} {
}

auto environment::collection() -> util::collection_group& {
	return collection_group_;
}

auto environment::cluster() -> ivm::cluster& {
	return cluster_;
}

auto environment::nenv() -> napi_env {
	return env_;
}

auto environment::isolate() -> v8::Isolate* {
	return isolate_;
}

auto environment::get(napi_env env) -> environment& {
	return *static_cast<environment*>(napi_invoke_checked(napi_get_instance_data, env));
}

} // namespace ivm
