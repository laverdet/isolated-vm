module isolated_v8.realm;
import isolated_v8.agent;
import isolated_v8.remote;
import v8;

namespace isolated_v8 {

// realm
realm::realm(agent::lock& agent, v8::Local<v8::Context> context) :
		context_{make_shared_remote(agent, context)} {}

auto realm::make(agent::lock& agent) -> realm {
	auto* isolate = agent->isolate();
	auto latch = agent->random_seed_latch();
	return realm{agent, v8::Context::New(isolate)};
}

// realm::scope
realm::scope::scope(agent::lock& agent, realm& realm) :
		js::iv8::context_managed_lock{agent, realm.context_->deref(agent)},
		agent_lock_{&agent} {}

auto realm::scope::agent() const -> agent::lock& {
	return *agent_lock_;
}

auto realm::scope::accept_remote_handle(remote_handle& remote) noexcept -> void {
	return agent_lock_->accept_remote_handle(remote);
}

auto realm::scope::remote_expiration_task() const -> reset_handle_type {
	return agent_lock_->remote_expiration_task();
}

} // namespace isolated_v8
