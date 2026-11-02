export module v8_js:platform.worker_scheduler;
import :platform.scheduler;
import :platform.thread_cache;
import std;
import util;

namespace js::iv8::platform {

template <class Queue>
struct worker_storage : scheduler_storage<Queue> {
		unsigned idle = 0;
};

// Dispatches tasks from a queue to background threads requested from the cache as needed
export template <class Queue>
class worker_scheduler : public scheduler_of<Queue, worker_storage<Queue>> {
	private:
		using scheduler_type = scheduler_of<Queue, worker_storage<Queue>>;
		friend scheduler_type;

	public:
		using scheduler_type::scheduler_type;
		worker_scheduler(const worker_scheduler&) = delete;
		~worker_scheduler();
		auto operator=(const worker_scheduler&) -> worker_scheduler& = delete;

		auto signal(std::invocable<Queue&> auto operation) -> void;
		[[nodiscard]] auto thread_count() const -> unsigned { return this->cache().concurrency(); }

	private:
		auto run(auto& lock, runner_thread& thread, util::inplace_stop_token stop_token) -> void;
};

// ---

template <class Queue>
worker_scheduler<Queue>::~worker_scheduler() {
	this->terminate();
	this->join();
}

template <class Queue>
auto worker_scheduler<Queue>::signal(std::invocable<Queue&> auto operation) -> void {
	auto lock = this->storage().write_notify(scheduler_type::has_tasks);
	operation(lock->queue);
	if (lock->idle == 0 && lock->threads < thread_count()) {
		auto kind = lock->threads == 0 ? thread_cache::request_kind::hard : thread_cache::request_kind::soft;
		this->request_thread(*lock, kind);
	}
}

template <class Queue>
auto worker_scheduler<Queue>::run(auto& lock, runner_thread& thread, util::inplace_stop_token stop_token) -> void {
	while (!stop_token.stop_requested()) {
		++lock->idle;
		auto task = this->wait_for_task(lock, thread);
		--lock->idle;
		if (task) {
			this->run_task(lock, *std::move(task));
		} else if (task.error()) {
			break;
		}
	}
	// nb: Reassigned by the cache at the same time new work was assigned
	if (!stop_token.stop_requested() && lock->threads == 1 && !lock->queue.empty()) {
		this->request_thread(*lock, thread_cache::request_kind::hard);
	}
}

} // namespace js::iv8::platform
