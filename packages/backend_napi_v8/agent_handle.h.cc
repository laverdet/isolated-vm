export module backend_napi_v8:agent_handle;
import :lock;
import :runtime;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// This storage is managed by the external isolate, not napi.
class agent_environment {
	public:
		explicit agent_environment(const js::iv8::isolated::agent_lock& /*lock*/) {}

		auto initialize(const agent_lock& lock) -> void { runtime_interface_.emplace(std::monostate{}, lock); }

		auto instantiate_runtime(const realm_scope& lock) -> auto {
			return runtime_interface_->instantiate(lock);
		}

	private:
		std::optional<runtime_interface> runtime_interface_;
};

// Internal handle which eventually holds a weak reference to the isolate
using agent_handle = js::iv8::isolated::agent_handle_of<agent_environment>;

// Wrapper for 'context_scope_operation' which also saves the shared context remote
thread_local const js::iv8::shared_remote<v8::Context>* shared_realm_remote{};

auto realm_scope_operation(const agent_lock& lock, const js::iv8::shared_remote<v8::Context>& realm, auto operation) -> decltype(auto) {
	const auto* previous = std::exchange(shared_realm_remote, &realm);
	auto scope = util::scope_exit{[ & ] { shared_realm_remote = previous; }};
	return context_scope_operation(lock, realm->deref(lock), std::move(operation));
}

auto get_context_shared_remote(const realm_scope& /*lock*/) -> const js::iv8::shared_remote<v8::Context>& {
	if (shared_realm_remote == nullptr) {
		throw std::logic_error{"No current realm"};
	}
	return *shared_realm_remote;
}

} // namespace backend_napi_v8
