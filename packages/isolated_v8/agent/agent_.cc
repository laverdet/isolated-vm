module;
#include <concepts>
#include <functional>
#include <memory>
#include <stop_token>
#include <utility>
#include <variant>
export module isolated_v8:agent_handle;
export import :agent_fwd;
export import :agent_host;
import :cluster;
import :foreground_runner;
import ivm.utility;
import v8;

namespace isolated_v8 {

// A `lock` is a simple holder for an `agent_host` which proves that we are executing in
// the isolate context.
class agent::lock
		: util::non_moveable,
			public util::pointer_facade,
			public js::iv8::isolate_lock_witness,
			public remote_handle_lock {
	public:
		lock(js::iv8::isolate_lock_witness& witness, std::shared_ptr<agent_host> host);
		~lock();

		auto operator->(this auto& self) -> auto* { return self.host_.get(); }
		static auto get_current() -> lock&;

	private:
		std::shared_ptr<agent_host> host_;
		lock* previous_;
};

// ---

auto agent::make(std::invocable<const agent::lock&, agent> auto fn, cluster& cluster, behavior_params params) -> void {
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
				std::invoke(std::move(fn), agent::lock{isolate_lock, std::move(host)}, std::move(agent));
			}
			// nb: `runner` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return std::move(runner);
		}
	);
}

auto agent::schedule(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const agent::lock&, decltype(args)...> {
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
				task(agent::lock{isolate_lock, std::move(host)}, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner(), std::move(task_with_lock));
	}
}

auto agent::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, const agent::lock&, decltype(args)...> {
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
				auto lock = agent::lock{isolate_lock, host};
				task(std::move(stop_token), lock, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
