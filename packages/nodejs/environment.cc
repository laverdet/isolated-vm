module;
#include <expected>
export module ivm.node:environment;
import ivm.isolated_v8;
import ivm.utility;
import napi;
import v8;

namespace ivm {

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment {
	public:
		environment();

		auto cluster() -> ivm::cluster&;
		auto collection() -> collection_group&;
		auto context() -> v8::Local<v8::Context>;
		auto isolate() -> v8::Isolate*;

		static auto get(Napi::Env env) -> environment&;

	private:
		ivm::cluster cluster_;
		v8::Isolate* isolate_;
		v8::Persistent<v8::Context> context_;
		collection_group collection_group_;
};

} // namespace ivm

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
