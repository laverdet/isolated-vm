module;
#include <cassert>
module isolated_v8;
import :agent_host;
import :remote;
import v8;

namespace isolated_v8 {

// realm
realm::realm(const agent_lock& agent, v8::Local<v8::Context> context) :
		context_{make_shared_remote(agent, context)} {}

auto realm::make(const agent_lock& agent) -> realm {
	auto* isolate = agent->isolate();
	auto latch = agent->random_seed_latch();
	return realm{agent, v8::Context::New(isolate)};
}

auto realm::lock(const agent_lock& agent) const -> js::iv8::context_managed_lock {
	return js::iv8::context_managed_lock{agent, context_->deref(agent)};
}

// realm::scope
realm::scope::scope(const agent_lock& agent, const context_lock_witness& lock) :
		context_lock_witness{lock},
		remote_handle_lock{util::slice_cast<remote_handle_lock>(agent)},
		agent_lock_{agent} {}

auto realm::scope::agent() const -> const agent_lock& {
	return agent_lock_;
}

} // namespace isolated_v8
