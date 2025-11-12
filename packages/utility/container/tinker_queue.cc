module;
#include <algorithm>
#include <cassert>
#include <concepts>
#include <deque>
#include <utility>
export module ivm.utility:container.tinker_queue;

namespace util {

// A queue that lets you pick elements out the middle for some reason
export template <class Value>
class tinker_queue {
	public:
		static_assert(std::constructible_from<Value>);
		static_assert(std::constructible_from<bool, Value>);

		using container_type = std::deque<Value>;
		using value_type = Value;

		[[nodiscard]] auto empty() const -> bool { return size() == 0; }
		[[nodiscard]] auto size() const -> size_t { return queue_.size() - empty_middle_values_; }
		// nb: You don't want a `clear` method here because you want tasks to be destroyed in the order
		// they were added
		auto emplace(auto&&... args) -> void;
		auto front(this auto& self) -> auto& { return self.queue_.front(); }
		auto pop() -> void;
		auto take(std::invocable<const value_type&> auto predicate) -> value_type;

	private:
		container_type queue_;
		size_t empty_middle_values_{};
};

// ---

template <class Value>
auto tinker_queue<Value>::emplace(auto&&... args) -> void {
	queue_.emplace_back(std::forward<decltype(args)>(args)...);
	assert(queue_.back());
}

template <class Value>
auto tinker_queue<Value>::pop() -> void {
	queue_.pop_front();
	for (auto& value : queue_) {
		if (value) {
			return;
		} else {
			queue_.pop_front();
			--empty_middle_values_;
		}
	}
}

template <class Value>
auto tinker_queue<Value>::take(std::invocable<const value_type&> auto predicate) -> value_type {
	auto it = std::ranges::find_if(const_cast<const container_type&>(queue_), predicate);
	if (it == queue_.begin()) {
		auto result = std::move(*it);
		pop();
		return result;
	} else if (it == queue_.end()) {
		return {};
	} else {
		auto result = std::exchange(std::move(*it), {});
		assert(result);
		++empty_middle_values_;
		return result;
	}
}

} // namespace util
