module;
#include <cassert>
#include <memory>
#include <stop_token>
export module v8_js:platform.task;
import ivm.utility;
import v8;

namespace js::iv8::platform {
// Forward `scheduler`'s stop_token through `v8::Task` virtual dispatch
inline thread_local std::stop_token* thread_stop_token;

// v8 task types
using idle_task = std::unique_ptr<v8::IdleTask>;
using task_type = std::unique_ptr<v8::Task>;

// Adapter for `std::unique_ptr<v8::Task>` which is invocable and boolable
struct invocable_task_type {
	public:
		invocable_task_type() = default;
		explicit invocable_task_type(task_type task) : task_{std::move(task)} {}

		auto operator()(std::stop_token stop_token) -> void {
			thread_stop_token = &stop_token;
			task_->Run();
		}

		explicit operator bool() const { return static_cast<bool>(task_); }

	private:
		task_type task_;
};

// Allow lambda-style callbacks to be called with the same virtual dispatch as `v8::Task`
template <std::invocable<std::stop_token> Invocable>
class task_of final : public v8::Task {
	public:
		explicit task_of(Invocable task) : task_{std::move(task)} {}
		auto Run() -> void final { task_(*thread_stop_token); }

	private:
		Invocable task_;
};

template <class... Args>
auto make_task_of(std::invocable<std::stop_token, Args...> auto callback, Args&&... args) -> std::unique_ptr<v8::Task> {
	auto task = [ & ]() -> auto {
		if constexpr (sizeof...(Args) == 0) {
			return std::move(callback);
		} else {
			// bind the arguments but move `stop_token` back to the first parameter
			return util::bind{
				[ & ](auto callback, std::remove_reference_t<Args>&... args, std::stop_token stop_token) -> void {
					return callback(std::move(stop_token), std::move(args)...);
				},
				std::move(callback),
				std::forward<Args>(args)...
			};
		}
	}();
	return std::make_unique<task_of<decltype(task)>>(std::move(task));
}

} // namespace js::iv8::platform
