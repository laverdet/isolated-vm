module;
#include <cstdio>
export module ivm.isolated_v8:realm;
import :agent;
import v8;

namespace ivm {

export class realm : non_copyable {
	public:
		class scope;

		realm() = delete;
		realm(v8::Isolate* isolate, v8::Local<v8::Context> context);
		static auto make(agent::lock& lock) -> realm;

	private:
		v8::Global<v8::Context> context;
};

class realm::scope : non_copyable {
	public:
		scope() = delete;
		explicit scope(agent::lock& lock, realm& realm);

		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

	private:
		v8::Isolate* isolate_;
		v8::Local<v8::Context> context_;
		v8::Context::Scope context_scope;
};

realm::realm(v8::Isolate* isolate, v8::Local<v8::Context> context) :
		context{isolate, context} {
}

auto realm::make(agent::lock& lock) -> realm {
	auto* isolate = lock->isolate();
	auto latch = lock->random_seed_latch();
	return realm{isolate, v8::Context::New(isolate)};
}

realm::scope::scope(agent::lock& lock, realm& realm) :
		isolate_{lock->isolate()},
		context_{v8::Local<v8::Context>::New(isolate_, realm.context)},
		context_scope{context_} {
}

} // namespace ivm
