module;
#include <optional>
export module isolated_v8:realm;
import :agent_host;
import :remote_handle;
import :remote;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

export class realm {
	public:
		class scope;
		class witness_scope;

		realm() = delete;
		realm(agent::lock& agent, v8::Local<v8::Context> context);
		static auto get(v8::Local<v8::Context> context) -> realm&;
		static auto make(agent::lock& agent) -> realm;

	private:
		shared_remote<v8::Context> context_;
};

class realm::scope
		: util::non_moveable,
			public js::iv8::context_implicit_witness_lock,
			public remote_handle_lock {
	protected:
		// enters the context
		scope(agent::lock& agent, realm& realm, v8::Local<v8::Context> context);
		// does not enter the context
		scope(agent::lock& agent, v8::Local<v8::Context> context);

	public:
		scope() = delete;
		scope(agent::lock& agent, realm& realm);

		[[nodiscard]] auto agent() const -> agent::lock&;

		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;

	private:
		agent::lock* agent_lock_;
		std::optional<js::iv8::context_managed_lock> context_lock_;
};

class realm::witness_scope : public realm::scope {
	public:
		witness_scope(agent::lock& agent, v8::Local<v8::Context> context);
};

} // namespace isolated_v8
