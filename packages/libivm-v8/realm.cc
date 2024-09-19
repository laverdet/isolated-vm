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
		agent_lock_{&agent},
		context_{context} {
}

auto realm::scope::agent() const -> agent::lock& {
	return *agent_lock_;
}

auto realm::scope::context() const -> v8::Local<v8::Context> {
	return context_;
}

auto realm::scope::isolate() const -> v8::Isolate* {
	return (*agent_lock_)->isolate();
}

realm::managed_scope::managed_scope(agent::lock& agent, realm& realm) :
		scope{agent, v8::Local<v8::Context>::New(agent->isolate(), realm.context)},
		context_scope{context()} {
}

} // namespace ivm
