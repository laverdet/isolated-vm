module;
#include <concepts>
export module isolated_v8:realm;
import :agent_handle;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

class realm_lock;

export class realm {
	public:
		using scope = js::iv8::context_lock_witness_of<agent_host, agent_remote_handle_lock, agent_collected_handle_lock>;

		realm() = delete;
		realm(const agent_lock& agent, v8::Local<v8::Context> context);

		auto invoke(const agent_lock& agent, std::invocable<const realm::scope&> auto task) const;

		[[nodiscard]] static auto get(const agent_lock& agent) -> realm&;
		[[nodiscard]] static auto get(v8::Local<v8::Context> context, const agent_lock& agent) -> realm&;
		[[nodiscard]] static auto get(v8::Local<v8::Context> context) -> realm&;
		[[nodiscard]] static auto make(const agent_lock& agent) -> realm;

	private:
		[[nodiscard]] auto lock(const agent_lock& agent) const -> js::iv8::context_managed_lock;

		js::iv8::shared_remote<v8::Context> context_;
};

// ---

auto realm::invoke(const agent_lock& agent, std::invocable<const realm::scope&> auto task) const {
	auto context_lock = lock(agent);
	return task(realm::scope{context_lock, *agent});
}

} // namespace isolated_v8
