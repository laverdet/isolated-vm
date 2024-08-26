module ivm.isolated_v8;
import :agent;
import :realm;
import v8;

namespace ivm {

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
