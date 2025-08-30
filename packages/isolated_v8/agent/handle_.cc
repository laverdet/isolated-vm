module;
#include <functional>
#include <memory>
#include <stop_token>
#include <variant>
export module isolated_v8:agent_handle;
export import :agent_fwd;
export import :agent_host;
import :clock;
import :cluster;
import :remote_handle;
import :foreground_runner;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// An `agent_lock` is a simple holder for an `agent_host` which proves that we are executing in the
// isolate context.
export class agent_lock
		: util::non_moveable,
			public util::pointer_facade,
			public js::iv8::isolate_lock_witness,
			public remote_handle_lock {
	public:
		agent_lock(const js::iv8::isolate_lock_witness& witness, agent_host& host);
		auto operator->(this auto& self) -> auto* { return std::addressof(self.host_.get()); }

	private:
		std::reference_wrapper<agent_host> host_;
};

// This keeps the `weak_ptr` in `agent_handle` alive. The `agent_host` maintains a `weak_ptr` to
// this and can "sever" the client connection if it needs to.
export class agent_severable {
	public:
		explicit agent_severable(std::shared_ptr<agent_host> host);

		auto sever() -> void;

	private:
		std::atomic<std::shared_ptr<agent_host>> host_;
};

// The base `agent_handle` class holds a weak reference to a `agent_host`. libivm directly controls
// the lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent_handle {
	public:
		agent_handle(const std::shared_ptr<agent_host>& host, std::shared_ptr<agent_severable> severable);

		auto schedule(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const agent_lock&, decltype(args)...>;
		auto schedule_async(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const std::stop_token&, const agent_lock&, decltype(args)...>;

		static auto make(std::invocable<const agent_lock&, agent_handle> auto fn, cluster& cluster, behavior_params params) -> void;

	private:
		std::weak_ptr<agent_host> host_;
		std::shared_ptr<agent_severable> severable_;
};

// ---

auto agent_handle::make(std::invocable<const agent_lock&, agent_handle> auto fn, cluster& cluster, behavior_params params) -> void {
	auto runner = std::make_shared<foreground_runner>(cluster.scheduler());
	foreground_runner::schedule_client_task(
		runner,
		[ &cluster, // I *think* this is safe because if `cluster` is destroyed the lambda won't be invoked
			params,
			runner,
			fn = std::move(fn) ](
			const std::stop_token& /*stop_token*/
		) mutable {
			{
				auto host = std::make_shared<agent_host>(cluster.scheduler(), runner, params);
				auto isolate_lock = js::iv8::isolate_execution_lock{host->isolate()};
				auto agent = agent_host::make_handle(host);
				std::invoke(std::move(fn), agent_lock{isolate_lock, *host}, std::move(agent));
			}
			// nb: `runner` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return std::move(runner);
		}
	);
}

auto agent_handle::schedule(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const agent_lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto task_with_lock =
			[ host,
				task = std::move(task),
				... args = std::move(args) ](
				const std::stop_token& /*stop_token*/
			) mutable {
				auto isolate_lock = js::iv8::isolate_execution_lock{host->isolate()};
				std::visit([](auto& clock) { clock.begin_tick(); }, host->clock());
				task(agent_lock{isolate_lock, *host}, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner(), std::move(task_with_lock));
	}
}

auto agent_handle::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, const agent_lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto& scheduler = host->async_scheduler();
		scheduler(
			[](
				std::stop_token stop_token,
				const std::shared_ptr<agent_host>& host,
				auto task,
				auto&&... args
			) {
				auto isolate_lock = js::iv8::isolate_execution_lock{host->isolate()};
				task(std::move(stop_token), agent_lock{isolate_lock, *host}, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
