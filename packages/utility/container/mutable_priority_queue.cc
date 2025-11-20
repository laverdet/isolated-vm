module;
#include <concepts>
#include <queue>
#include <utility>
export module util:container.mutable_priority_queue;

namespace util {

template <class Value>
struct mutable_entry {
		mutable_entry() = default;
		explicit mutable_entry(auto&&... args)
			requires std::constructible_from<Value, decltype(args)...>
				: value_{std::forward<decltype(args)>(args)...} {}
		auto operator*() const -> Value& { return value_; }
		auto operator<=>(const mutable_entry&) const = default;

	private:
		mutable Value value_;
};

// Allows the top element of `std::priority_queue` to be mutable
export template <class Value>
class mutable_priority_queue : public std::priority_queue<mutable_entry<Value>> {
	private:
		using priority_queue = std::priority_queue<mutable_entry<Value>>;

	public:
		using value_type = Value;

		auto top() -> value_type& { return *priority_queue::top(); }
		auto top() const -> const value_type& { return *priority_queue::top(); }
};

} // namespace util
