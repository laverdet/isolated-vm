module;
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
export module ivm.isolated_v8:platform.foreground_runner;
import :platform.task_runner;
import ivm.utility;
import v8;

using namespace std::chrono;

namespace ivm {

class foreground_runner : public task_runner::task_runner_of<foreground_runner> {
	private:
		class storage : util::non_copyable {
			public:
				friend foreground_runner;
				explicit storage(int& nesting_depth);

				auto acquire() -> task_type;
				auto flush_delayed() -> void;
				[[nodiscard]] auto should_resume() const -> bool;

			private:
				std::priority_queue<delayed_task> delayed_tasks_;
				std::deque<std::pair<nestability, task_type>> tasks_;
				int* nesting_depth_;
				bool should_resume_{};
		};
		using lockable_storage = util::lockable<storage, std::mutex, std::condition_variable_any>;
		using write_waitable_type = lockable_storage::write_waitable_type<bool (storage::*)() const>;

	public:
		foreground_runner();
		auto execute() -> bool;
		auto post_delayed(task_type task, steady_clock::time_point timeout) -> void final;
		auto post_non_nestable_delayed(task_type task, steady_clock::time_point timeout) -> void;
		auto post_non_nestable(task_type task) -> void;
		auto post(task_type task) -> void final;
		auto schedule_non_nestable(task_type task) -> void;
		auto scope(std::invocable<write_waitable_type> auto body) -> decltype(auto);

		int nesting_depth_{};
		lockable_storage storage_;
};

auto foreground_runner::scope(std::invocable<write_waitable_type> auto body) -> decltype(auto) {
	++nesting_depth_;
	auto scope = util::scope_exit{[ this ] { --nesting_depth_; }};
	return body(storage_.write_waitable(&storage::should_resume));
}

} // namespace ivm
