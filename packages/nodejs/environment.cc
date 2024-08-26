module ivm.node;
import :environment;
import ivm.isolated_v8;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm {

environment::environment() :
		isolate_{v8::Isolate::GetCurrent()},
		context_{isolate_, isolate_->GetCurrentContext()} {
	context_.SetWeak();
}

auto environment::collection() -> collection_group& {
	return collection_group_;
}

auto environment::cluster() -> ivm::cluster& {
	return cluster_;
}

auto environment::context() -> v8::Local<v8::Context> {
	return context_.Get(isolate_);
}

auto environment::isolate() -> v8::Isolate* {
	return isolate_;
}

auto environment::get(Napi::Env env) -> environment& {
	return *env.GetInstanceData<environment>();
}

} // namespace ivm
