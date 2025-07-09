module;
#include <concepts>
#include <functional>
#include <memory>
#include <stop_token>
#include <utility>
#include <variant>
export module isolated_v8:agent;
export import :agent_fwd;
export import :agent_host;
export import :agent_lock;
import :cluster;
import :foreground_runner;
import ivm.utility;
import v8;

namespace isolated_v8 {

auto agent::make(std::invocable<agent::lock&, agent> auto fn, cluster& cluster, behavior_params params) -> void {
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
				auto agent_host = std::make_shared<agent::host>(cluster.scheduler(), runner, params);
				auto agent_lock = agent::lock{agent_host};
				// TODO: HandleScope should probably be a part of the agent lock
				auto handle_scope = v8::HandleScope{agent_host->isolate_.get()};
				auto agent = agent::host::make_handle(std::move(agent_host));
				std::invoke(std::move(fn), agent_lock, std::move(agent));
			}
			// nb: `runner` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return std::move(runner);
		}
	);
}

auto agent::schedule(auto task, auto... args) -> void
	requires std::invocable<decltype(task), agent::lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto task_with_lock =
			[ host,
				task = std::move(task),
				... args = std::move(args) ](
				const std::stop_token& /*stop_token*/
			) mutable {
				agent::lock agent_lock{host};
				v8::HandleScope handle_scope{host->isolate_.get()};
				std::visit([](auto& clock) { clock.begin_tick(); }, host->clock_);
				task(agent_lock, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner_, std::move(task_with_lock));
	}
}

auto agent::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, agent::lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto& scheduler = host->async_scheduler_;
		scheduler(
			[](
				std::stop_token stop_token,
				const std::shared_ptr<agent::host>& host,
				auto task,
				auto&&... args
			) {
				agent::lock agent_lock{host};
				v8::HandleScope handle_scope{host->isolate_.get()};
				task(std::move(stop_token), agent_lock, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
