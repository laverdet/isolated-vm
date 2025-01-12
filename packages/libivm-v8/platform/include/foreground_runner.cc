module;
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
export module ivm.isolated_v8:platform.foreground_runner;
import :platform.task_runner;
import :scheduler;
import ivm.utility;
import v8;

using namespace std::chrono;

namespace isolated_v8 {

class foreground_runner : public task_runner::task_runner_of<foreground_runner> {
	public:
		class scope;
		friend scope;

	private:
		class storage : util::non_copyable {
			public:
				friend foreground_runner;

				auto acquire() -> task_type;
				auto flush_delayed() -> void;
				[[nodiscard]] auto should_resume() const -> bool;

			private:
				std::priority_queue<delayed_task> delayed_tasks_;
				std::deque<std::pair<nestability, task_type>> tasks_;
				int nesting_depth_;
				// nb: `foreground_thread` must be invoked once within the thread that allocates it
				bool idle_{false};
		};
		using lockable_storage = util::lockable<storage, std::mutex, std::condition_variable_any>;
		using write_waitable_type = lockable_storage::write_waitable_type<bool (storage::*)() const>;

	public:
		explicit foreground_runner(scheduler::runner<{}>& runner);

		auto foreground_thread(const std::stop_token& stop_token) -> void;
		auto post_delayed(task_type task, steady_clock::time_point timeout) -> void final;
		auto post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void;
		auto post_non_nestable(task_type task) -> void;
		auto post(task_type task) -> void final;

		static auto schedule_non_nestable(const std::shared_ptr<foreground_runner>& self, task_type task) -> void;

	private:
		lockable_storage storage_;
		scheduler::handle scheduler_;
};

class foreground_runner::scope : public foreground_runner::write_waitable_type {
	public:
		explicit scope(foreground_runner& runner);
		~scope();
};

} // namespace isolated_v8
