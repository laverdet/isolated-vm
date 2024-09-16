module;
#include <concepts>
#include <memory>
export module ivm.isolated_v8:agent.lock;
import ivm.utility;

namespace ivm {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;
		class foreground_runner;
		class storage;

		explicit agent(
			const std::shared_ptr<host>& host,
			const std::shared_ptr<foreground_runner>& task_runner
		);

		auto schedule(std::invocable<lock&> auto&& task) -> void;

	private:
		std::weak_ptr<host> host_;
		std::shared_ptr<foreground_runner> task_runner_;
};

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : util::non_moveable {
	public:
		explicit lock(host& host);
		~lock();

		auto operator*() -> host& { return host_; }
		auto operator*() const -> const host& { return host_; }
		auto operator->() -> host* { return &host_; }
		auto operator->() const -> const host* { return &host_; }

		static auto expect() -> lock&;

	private:
		host& host_;
		lock* prev_;
};

} // namespace ivm
