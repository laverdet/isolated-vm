module;
#include <cassert>
#include <chrono>
#include <deque>
#include <optional>
#include <queue>
export module v8_js:platform.task_queue;
import :platform.task;
import v8;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// Holder of task + nestability
export struct task_entry {
		enum class nestability : bool {
			non_nestable,
			nestable
		};

		// nb: Mutable for use in `std::priority_queue`
		mutable task_type task;
		nestability nestability;
};

// Holder of task + timeout + nestability
export struct delayed_task_entry : task_entry {
		struct timeout_predicate;
		auto operator<=>(const delayed_task_entry& right) const -> std::strong_ordering;
		auto operator<=>(steady_clock::time_point right) const -> std::strong_ordering;

		steady_clock::time_point timeout;
};

// Queue of tasks
export class task_queue {
	public:
		[[nodiscard]] auto empty() const -> bool;
		auto clear() -> void;
		auto pop_nestable() -> task_type;
		auto pop() -> task_type;
		auto push(task_entry task) -> void;

	private:
		std::deque<task_entry> tasks_;
		size_t empty_tasks_{};
};

// Queue of delayed tasks
export class delayed_task_queue {
	public:
		[[nodiscard]] auto empty() const -> bool;
		auto clear() -> void;
		auto pop_if(steady_clock::time_point time) -> std::optional<task_entry>;
		auto push(delayed_task_entry task) -> void;

	private:
		std::priority_queue<delayed_task_entry> tasks_;
};

} // namespace js::iv8::platform
