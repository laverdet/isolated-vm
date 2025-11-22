module;
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <span>
#include <utility>
export module util:container.segmented_priority_queue;

namespace util {

// Span which expands or contracts to include or possibly exclude the given members.
export template <class Type>
class ratchet_span : public std::span<Type> {
	public:
		using span_type = std::span<Type>;
		using span_type::begin;
		using span_type::end;
		using span_type::span_type;
		using iterator = span_type::iterator;

		auto erase(auto it) -> void;
		auto insert(auto it) -> void;
		static auto make_iterator(auto it) -> iterator;
};

// N number of queues, which tracks which ones currently may have elements
export template <class Queue, size_t Size>
class segmented_priority_queue {
	public:
		using container_type = Queue;
		using queues_type = std::array<container_type, Size>;
		using value_type = container_type::value_type;

		[[nodiscard]] auto containers() const -> std::span<const container_type> { return not_empty_; }
		[[nodiscard]] auto empty() const -> bool { return std::ranges::all_of(containers(), &Queue::empty); }
		[[nodiscard]] auto size() const -> size_t { return std::ranges::fold_left(containers(), 0UZ, std::plus{}, &Queue::size); }
		auto at(unsigned priority) -> container_type&;
		auto at(unsigned priority) const -> const container_type& { return queues_.at(priority); }
		auto containers() -> std::span<container_type> { return not_empty_; }
		auto emplace(unsigned priority, auto&&... args) -> void { at(priority).emplace(std::forward<decltype(args)>(args)...); }
		auto front(this auto& self) -> auto& { return std::ranges::find_if_not(self.containers(), &Queue::empty)->front(); }
		auto pop() -> void;

	private:
		queues_type queues_;
		mutable ratchet_span<container_type> not_empty_{queues_.end(), queues_.end()};
};

// ---

// `ratchet_span`
template <class Type>
auto ratchet_span<Type>::erase(auto it) -> void {
	auto local_it = make_iterator(it);
	auto next = std::next(local_it);
	if (end() == next) {
		*this = {begin(), local_it};
	} else if (begin() == local_it) {
		*this = {next, end()};
	}
}

template <class Type>
auto ratchet_span<Type>::insert(auto it) -> void {
	auto local_it = make_iterator(it);
	*this = {
		std::min(local_it, begin()),
		std::max(std::next(local_it), end())
	};
}

template <class Type>
auto ratchet_span<Type>::make_iterator(auto it) -> iterator {
	// `std::span<Type>::iterator` is implementation-defined, and in libc++ the constructor is
	// private.
	auto span = std::span<Type>{it, it};
	return span.begin();
}

// `segmented_priority_queue`
template <class Queue, size_t Size>
auto segmented_priority_queue<Queue, Size>::at(unsigned priority) -> container_type& {
	// We don't know what you're about to do with it, so assume that it will gain an element
	auto& queue = queues_.at(priority);
	not_empty_.insert(&queue);
	return queue;
}

template <class Queue, size_t Size>
auto segmented_priority_queue<Queue, Size>::pop() -> void {
	for (auto& queue : not_empty_) {
		if (queue.empty()) {
			not_empty_.erase(&queue);
		} else {
			queue.pop();
			if (queue.empty()) {
				not_empty_.erase(&queue);
			}
			return;
		}
	}
}

} // namespace util
