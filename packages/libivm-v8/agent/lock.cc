module;
export module ivm.isolated_v8:agent.lock;
import :agent.fwd;
import ivm.utility;

namespace ivm {

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : non_moveable {
	public:
		explicit lock(host& agent_host) :
				agent_host{agent_host} {}

		auto operator*() -> host& { return agent_host; }
		auto operator*() const -> const host& { return agent_host; }
		auto operator->() -> host* { return &agent_host; }
		auto operator->() const -> const host* { return &agent_host; }

	private:
		host& agent_host;
};

} // namespace ivm
