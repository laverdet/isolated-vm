module;
#include <memory>
module ivm.isolated_v8;
import :agent;
import :realm;
import :remote;
import v8;

namespace isolated_v8 {

realm::realm(agent::lock& agent_lock, v8::Local<v8::Context> context) :
		context_{make_shared_remote(agent_lock, context)} {}

auto realm::make(agent::lock& agent) -> realm {
	auto* isolate = agent->isolate();
	auto latch = agent->random_seed_latch();
	return realm{agent, v8::Context::New(isolate)};
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
		scope{agent, realm.context_->deref(agent)},
		context_scope{context()} {
}

} // namespace isolated_v8
