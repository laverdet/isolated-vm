module;
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
export module ivm.isolated_v8:agent.task_runner;
import :agent.lock;
import :platform.task_runner;
import ivm.utility;
import v8;

using namespace std::chrono;

namespace ivm {

// Allow lambda-style callbacks to be called with the same virtual dispatch as `v8::Task`
template <std::invocable<agent::lock&> Invocable>
struct task_of : v8::Task {
		explicit task_of(Invocable&& task) :
				task_{std::forward<Invocable>(task)} {}

		auto Run() -> void final {
			task_(agent::lock::expect());
		}

	private:
		[[no_unique_address]] Invocable task_;
};

class agent::foreground_runner : public task_runner::task_runner_of<foreground_runner> {
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
		auto schedule_non_nestable(std::invocable<lock&> auto task) -> void;
		auto scope(std::invocable<write_waitable_type> auto body) -> decltype(auto);

		int nesting_depth_{};
		lockable_storage storage_;
};

auto agent::foreground_runner::schedule_non_nestable(std::invocable<lock&> auto task) -> void {
	auto lock = storage_.write_notify();
	lock->tasks_.emplace_back(nestability::non_nestable, std::make_unique<task_of<decltype(task)>>(std::move(task)));
	lock->should_resume_ = true;
}

auto agent::foreground_runner::scope(std::invocable<write_waitable_type> auto body) -> decltype(auto) {
	++nesting_depth_;
	auto scope = util::scope_exit{[ this ] { --nesting_depth_; }};
	return body(storage_.write_waitable(&storage::should_resume));
}

auto agent::schedule(std::invocable<lock&> auto&& task) -> void {
	task_runner_->schedule_non_nestable(std::forward<decltype(task)>(task));
}

} // namespace ivm
