module;
#include <concepts>
#include <functional>
export module isolated_v8:realm;
import :agent_handle;
import :remote_handle;
import :remote;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

class realm_lock;

export class realm {
	public:
		class scope;

		realm() = delete;
		realm(const agent_lock& agent, v8::Local<v8::Context> context);

		auto invoke(const agent_lock& agent, std::invocable<const realm::scope&> auto task);

		[[nodiscard]] static auto get(const agent_lock& agent) -> realm&;
		[[nodiscard]] static auto get(v8::Local<v8::Context> context, const agent_lock& agent) -> realm&;
		[[nodiscard]] static auto get(v8::Local<v8::Context> context) -> realm&;
		[[nodiscard]] static auto make(const agent_lock& agent) -> realm;

	private:
		[[nodiscard]] auto lock(const agent_lock& agent) -> js::iv8::context_managed_lock;

		shared_remote<v8::Context> context_;
};

class realm::scope
		: util::non_moveable,
			public js::iv8::context_lock_witness,
			public remote_handle_lock {

	public:
		scope(const agent_lock& agent, const context_lock_witness& lock);

		[[nodiscard]] auto agent() const -> const agent_lock&;

	private:
		std::reference_wrapper<const agent_lock> agent_lock_;
};

// ---

auto realm::invoke(const agent_lock& agent, std::invocable<const realm::scope&> auto task) {
	return task(realm::scope{agent, lock(agent)});
}

} // namespace isolated_v8
