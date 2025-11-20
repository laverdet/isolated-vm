module;
#include <boost/intrusive/list.hpp>
#include <memory>
#include <stop_token>
#include <type_traits>
export module v8_js:isolated.cluster;
import :isolated.agent;
import :isolated.platform;
import util;

namespace js::iv8::isolated {

export class cluster {
	public:
		cluster() : platform_{isolated_platform::acquire()} {}
		auto make_agent(auto make_environment, behavior_params params, auto callback) -> void;
		auto release_agent_storage(std::shared_ptr<agent_storage> storage) -> void;

	private:
		using intrusive_no_size = boost::intrusive::constant_time_size<false>;
		using agent_storage_list_type = boost::intrusive::list<agent_storage, intrusive_no_size, agent_storage::intrusive_hook>;

		auto acquire_agent_storage() -> std::shared_ptr<agent_storage>;

		util::lockable<agent_storage_list_type> agent_storage_;
		platform::platform_handle platform_;
};

// ---

auto cluster::make_agent(auto make_environment, behavior_params params, auto callback) -> void {
	using environment_type = std::invoke_result_t<decltype(make_environment), agent_lock>;
	auto storage = acquire_agent_storage();
	auto& runner = storage->foreground_runner();
	runner.schedule_client_task(
		[ params ](
			std::stop_token /*stop_token*/,
			std::shared_ptr<agent_storage> storage,
			auto make_environment,
			auto callback
		) -> auto {
			auto host = std::make_shared<agent_host_of<environment_type>>(std::move(storage), params);
			auto isolate_lock = isolate_execution_lock{host->isolate()};
			host->emplace_environment(util::elide{[ & ]() -> environment_type {
				return make_environment(agent_lock{isolate_lock, *host});
			}});
			callback(agent_lock_of{isolate_lock, *host}, agent_handle_of{std::move(host)});
		},
		std::move(storage),
		std::move(make_environment),
		std::move(callback)
	);
}

} // namespace js::iv8::isolated
