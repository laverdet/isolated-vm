export module isolated_v8.realm;
import isolated_v8.agent;
import isolated_v8.remote_handle;
import isolated_v8.remote;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

export class realm {
	public:
		class managed_scope;
		class scope;

		realm() = delete;
		realm(agent::lock& agent, v8::Local<v8::Context> context);
		static auto get(v8::Local<v8::Context> context) -> realm&;
		static auto make(agent::lock& agent) -> realm;

	private:
		shared_remote<v8::Context> context_;
};

class realm::scope final
		: public js::iv8::context_managed_lock,
			public remote_handle_lock {
	public:
		scope() = delete;
		scope(agent::lock& agent, realm& realm);

		[[nodiscard]] auto agent() const -> agent::lock&;

		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;

	private:
		agent::lock* agent_lock_;
};

} // namespace isolated_v8
