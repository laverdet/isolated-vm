export module v8_js:isolated.cluster;
import :isolated.agent;
import :isolated.platform;
import std;
import util;

namespace js::iv8::isolated {

export class cluster {
	public:
		using synchronize_destroy = util::function_ref<auto(std::any)->void>;

		explicit cluster(synchronize_destroy destroy) :
				destroy_{destroy},
				platform_{isolated_platform::acquire()} {}

		template <class Environment>
		auto make_agent(std::type_identity<Environment> environment, behavior_params params, auto callback) -> void;
		auto release_agent_storage(std::shared_ptr<agent_storage> storage) -> void;

	private:
		using intrusive_no_size = boost::intrusive::constant_time_size<false>;
		using agent_storage_list_type = boost::intrusive::list<agent_storage, intrusive_no_size, agent_storage::intrusive_hook>;
		auto acquire_agent_storage() -> std::shared_ptr<agent_storage>;

		synchronize_destroy destroy_;
		util::lockable<agent_storage_list_type> agent_storage_;
		platform::platform_handle platform_;
};

// ---

template <class Environment>
auto cluster::make_agent(std::type_identity<Environment> /*environment*/, behavior_params params, auto callback) -> void {
	auto storage = acquire_agent_storage();
	auto& runner = storage->foreground_runner();
	runner.schedule_client_task(
		[ params ](
			std::stop_token /*stop_token*/,
			std::shared_ptr<agent_storage> storage,
			auto callback
		) -> auto {
			auto host = std::make_shared<agent_host_of<Environment>>(std::move(storage), params);
			auto isolate_lock = isolate_execution_lock{host->executor().isolate()};
			host->initialize_environment(agent_lock{isolate_lock, *host});
			auto lock = agent_lock_of{isolate_lock, *host};
			callback(lock, agent_handle_of{std::move(host)});
		},
		std::move(storage),
		std::move(callback)
	);
}

} // namespace js::iv8::isolated
