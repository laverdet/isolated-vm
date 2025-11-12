module;
#include <cassert>
#include <chrono>
#include <compare>
export module v8_js:platform.task_queue;
export import :platform.task;
import ivm.utility;
import v8;
using std::chrono::steady_clock;

namespace js::iv8::platform {

// Utilities to cast back and forth from enum type <-> numeric type
using priority_numeric_type = std::underlying_type_t<v8::TaskPriority>;

constexpr auto priority_numeric_from(v8::TaskPriority priority) -> priority_numeric_type {
	return static_cast<priority_numeric_type>(v8::TaskPriority::kMaxPriority) - static_cast<priority_numeric_type>(priority);
}

constexpr auto priority_enum_from(priority_numeric_type priority) -> v8::TaskPriority {
	return static_cast<v8::TaskPriority>(static_cast<priority_numeric_type>(v8::TaskPriority::kMaxPriority) - priority);
}

constexpr auto priority_count = static_cast<priority_numeric_type>(v8::TaskPriority::kMaxPriority) + 1;

// Boolean nestability enum sugar
enum class nestability : bool {
	non_nestable,
	nestable
};

// Holder of task entry & nestability
template <class Task>
struct nestable_entry : Task {
		nestable_entry() = default;
		explicit nestable_entry(nestability nestable, auto&&... rest) :
				Task{std::forward<decltype(rest)>(rest)...},
				nestable{nestable} {}

		auto operator<=>(const Task& right) const -> std::partial_ordering
			requires std::three_way_comparable<Task> {
			return static_cast<const Task&>(*this) <=> right;
		}

		nestability nestable{};
};

// Holder of task entry & timeout
template <class Task>
struct delayed_entry : Task {
		using clock_type = steady_clock;

		explicit delayed_entry(clock_type::time_point timeout, auto&&... rest) :
				Task{std::forward<decltype(rest)>(rest)...},
				timeout{timeout} {}

		auto operator<=>(const delayed_entry& right) const -> std::partial_ordering {
			return timeout <=> right.timeout;
		}

		auto operator<=>(clock_type::time_point right) const -> std::partial_ordering {
			return timeout <=> right;
		}

		clock_type::time_point timeout;
};

// Queue of delayed tasks
template <class Task>
class delayed_task_queue : public util::mutable_priority_queue<delayed_entry<Task>> {
	private:
		using queue_type = util::mutable_priority_queue<delayed_entry<Task>>;

	public:
		using value_type = queue_type::value_type;
		using clock_type = value_type::clock_type;

		[[nodiscard]] auto timeout() const -> clock_type::time_point;
		auto flush(auto& receiver, clock_type::time_point timeout) -> clock_type::time_point;

	private:
		queue_type tasks_;
};

// ---

// `delayed_task_queue`
template <class Task>
auto delayed_task_queue<Task>::timeout() const -> clock_type::time_point {
	return tasks_.empty() ? steady_clock::time_point::max() : tasks_.top().timeout;
}

template <class Entry>
auto delayed_task_queue<Entry>::flush(auto& receiver, clock_type::time_point timeout) -> clock_type::time_point {
	while (!tasks_.empty()) {
		auto& top = tasks_.top();
		if (top <= timeout) {
			receiver.emplace(std::move(top));
			tasks_.pop();
		} else {
			return top.timeout;
		}
	}
	return clock_type::time_point::max();
}

} // namespace js::iv8::platform
