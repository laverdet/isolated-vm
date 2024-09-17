module ivm.isolated_v8;
import :agent;
import :realm;
import v8;

namespace ivm {

realm::realm(v8::Isolate* isolate, v8::Local<v8::Context> context) :
		context{isolate, context} {
}

auto realm::make(agent::lock& agent) -> realm {
	auto* isolate = agent->isolate();
	auto latch = agent->random_seed_latch();
	return realm{isolate, v8::Context::New(isolate)};
}

realm::scope::scope(agent::lock& agent, v8::Local<v8::Context> context) :
		isolate_{agent->isolate()},
		context_{context} {
}

realm::managed_scope::managed_scope(agent::lock& agent, realm& realm) :
		scope{agent, v8::Local<v8::Context>::New(agent->isolate(), realm.context)},
		context_scope{context()} {
}

} // namespace ivm
