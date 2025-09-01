module;
#include <functional>
#include <memory>
#include <stop_token>
#include <variant>
export module isolated_v8:agent_handle;
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
		: public util::pointer_facade,
			public js::iv8::isolate_lock_witness,
			public remote_handle_lock {
	public:
		agent_lock(js::iv8::isolate_lock_witness witness, agent_host& host);
		auto operator->() const -> auto* { return std::addressof(host_.get()); }

	private:
		std::reference_wrapper<agent_host> host_;
};

export template <class Type>
class agent_lock_of : public agent_lock {
	public:
		agent_lock_of(js::iv8::isolate_lock_witness witness, agent_host_of<Type>& host) :
				agent_lock{witness, host} {}
		auto operator->() const -> auto* { return static_cast<agent_host_of<Type>*>(agent_lock::operator->()); }
};

// The base `agent_handle` class holds a weak reference to a `agent_host`. libivm directly controls
// the lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export template <class Type>
class agent_handle {
	private:
		using host_type = agent_host_of<Type>;
		agent_handle(const std::shared_ptr<host_type>& host, std::shared_ptr<agent_severable> severable);

	public:
		using lock = agent_lock_of<Type>;

		auto schedule(auto task, auto... args) -> void
			requires std::invocable<decltype(task), lock, decltype(args)...>;
		auto schedule_async(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const std::stop_token&, lock, decltype(args)...>;

		static auto make(cluster& cluster, auto make_environment, behavior_params params, std::invocable<lock, agent_handle> auto callback) -> void;

	private:
		std::weak_ptr<host_type> host_;
		std::shared_ptr<agent_severable> severable_;
};

// ---

// agent_handle
template <class Type>
agent_handle<Type>::agent_handle(const std::shared_ptr<host_type>& host, std::shared_ptr<agent_severable> severable) :
		host_{host},
		severable_{std::move(severable)} {}

template <class Type>
auto agent_handle<Type>::make(cluster& cluster, auto make_environment, behavior_params params, std::invocable<lock, agent_handle> auto callback) -> void {
	auto runner = std::make_shared<foreground_runner>(cluster.scheduler());
	foreground_runner::schedule_client_task(
		runner,
		[ &cluster, // I *think* this is safe because if `cluster` is destroyed the lambda won't be invoked
			runner,
			make_environment = std::move(make_environment),
			callback = std::move(callback),
			params = params ](
			const std::stop_token& /*stop_token*/
		) mutable {
			{
				auto host = std::make_shared<agent_host_of<Type>>(
					std::move(make_environment),
					cluster.scheduler(),
					runner,
					params
				);
				std::invoke(
					std::move(callback),
					lock{js::iv8::isolate_execution_lock{host->isolate()}, *host},
					agent_handle{host, agent_host::acquire_severable(host)}
				);
			}
			// nb: `runner` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return std::move(runner);
		}
	);
}

template <class Type>
auto agent_handle<Type>::schedule(auto task, auto... args) -> void
	requires std::invocable<decltype(task), lock, decltype(args)...> {
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
				task(lock{isolate_lock, *host}, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner(), std::move(task_with_lock));
	}
}

template <class Type>
auto agent_handle<Type>::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, lock, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto& scheduler = host->async_scheduler();
		scheduler(
			[](
				const std::stop_token& stop_token,
				const std::shared_ptr<host_type>& host,
				auto task,
				auto&&... args
			) {
				auto isolate_lock = js::iv8::isolate_execution_lock{host->isolate()};
				task(stop_token, lock{isolate_lock, *host}, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
