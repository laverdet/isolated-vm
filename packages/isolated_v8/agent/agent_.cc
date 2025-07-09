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
class agent::lock final
		: util::non_moveable,
			public util::pointer_facade,
			public js::iv8::isolate_managed_lock,
			public remote_handle_lock {
	public:
		explicit lock(std::shared_ptr<agent_host> host);
		~lock();

		auto operator->(this auto& self) -> auto* { return self.host_.get(); }
		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;
		static auto get_current() -> lock&;

	private:
		std::shared_ptr<agent_host> host_;
		lock* previous_;
};

// ---

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
				auto host = std::make_shared<agent_host>(cluster.scheduler(), runner, params);
				auto agent_lock = agent::lock{host};
				// TODO: HandleScope should probably be a part of the agent lock
				auto handle_scope = v8::HandleScope{host->isolate()};
				auto agent = agent_host::make_handle(std::move(host));
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
				v8::HandleScope handle_scope{host->isolate()};
				std::visit([](auto& clock) { clock.begin_tick(); }, host->clock());
				task(agent_lock, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner(), std::move(task_with_lock));
	}
}

auto agent::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, agent::lock&, decltype(args)...> {
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
				agent::lock agent_lock{host};
				v8::HandleScope handle_scope{host->isolate()};
				task(std::move(stop_token), agent_lock, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
