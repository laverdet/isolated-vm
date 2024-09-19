export module ivm.isolated_v8:realm;
import :agent_fwd;
import ivm.utility;
import v8;

namespace ivm {

export class realm : util::non_copyable {
	public:
		class managed_scope;
		class scope;

		realm() = delete;
		realm(v8::Isolate* isolate, v8::Local<v8::Context> context);
		static auto get(v8::Local<v8::Context> context) -> realm&;
		static auto make(agent::lock& agent) -> realm;

	private:
		v8::Global<v8::Context> context;
};

class realm::scope : util::non_copyable {
	public:
		scope() = delete;
		scope(agent::lock& agent, v8::Local<v8::Context> context);

		[[nodiscard]] auto agent() const -> agent::lock&;
		[[nodiscard]] auto context() const -> v8::Local<v8::Context>;
		[[nodiscard]] auto isolate() const -> v8::Isolate*;

	private:
		agent::lock* agent_lock_;
		v8::Local<v8::Context> context_;
};

class realm::managed_scope : public realm::scope {
	public:
		managed_scope(agent::lock& agent, realm& realm);

	private:
		v8::Context::Scope context_scope;
};

} // namespace ivm
