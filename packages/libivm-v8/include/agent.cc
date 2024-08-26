module;
#include <chrono>
#include <condition_variable>
#include <experimental/scope>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <variant>
export module ivm.isolated_v8:agent;
import :scheduler;
import ivm.utility;
import ivm.value;
import v8;

using namespace std::chrono;
using ivm::value::js_clock;

namespace ivm {

// The base `agent` class basically serves as a handle to an `agent::host`. This module directly
// controls the lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : non_copyable {
	public:
		class host;
		class lock;
		class storage;
		struct clock;
		using task_type = std::move_only_function<void(lock& lock)>;

		explicit agent(const std::shared_ptr<host>& host);

		auto schedule_task(task_type task) -> void;

	private:
		std::weak_ptr<host> host_;
};

// Various clock timing strategies for time observers in the agent
struct agent::clock {
		class base_clock;
		template <class Self>
		class base_clock_of;
		class deterministic;
		class microtask;
		class realtime;
		class system;
		using any_clock = std::variant<deterministic, microtask, realtime, system>;
};

class agent::clock::base_clock {
	public:
		auto begin_tick() -> void;

	protected:
		static auto steady_clock_offset(js_clock::time_point epoch) -> steady_clock::duration;
};

template <class Self>
class agent::clock::base_clock_of : public base_clock {
	public:
		auto clock_time_ms() -> int64_t;
};

template <class Self>
auto agent::clock::base_clock_of<Self>::clock_time_ms() -> int64_t {
	auto some_time_point = static_cast<Self*>(this)->clock_time();
	// You can't `clock_cast` from `steady_clock` but internally we redefine that epoch to be UTC via
	// `steady_clock_offset`, so casting the `duration` directly is correct for clocks that use that
	// reference.
	auto utc_duration = duration_cast<utc_clock::duration>(some_time_point.time_since_epoch());
	return static_cast<int64_t>(duration_cast<milliseconds>(utc_duration).count());
}

// Starts at a specified epoch and each query for the time will increment the clock by a constant
// amount.
class agent::clock::deterministic : public base_clock_of<deterministic> {
	public:
		deterministic(js_clock::time_point epoch, js_clock::duration increment);

		auto clock_time() -> utc_clock::time_point;

	private:
		utc_clock::time_point epoch_;
		utc_clock::duration increment_;
};

// During a microtask the clock will stay the same. It will update to the current time at the start
// of the next tick. Optionally you can specify a start epoch.
class agent::clock::microtask : public base_clock_of<microtask> {
	public:
		explicit microtask(std::optional<js_clock::time_point> maybe_epoch);

		auto begin_tick() -> void;
		auto clock_time() -> steady_clock::time_point;

	private:
		steady_clock::duration offset_;
		steady_clock::time_point time_point_;
};

// Realtime clock starting from the given epoch.
class agent::clock::realtime : public base_clock_of<realtime> {
	public:
		explicit realtime(js_clock::time_point epoch);

		auto clock_time() -> steady_clock::time_point;

	private:
		steady_clock::duration offset_;
};

// Pass through system time
class agent::clock::system : public base_clock_of<system> {
	public:
		auto clock_time() -> system_clock::time_point;
};

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
class agent::host : non_moveable {
	private:
		struct isolate_destructor {
				auto operator()(v8::Isolate* isolate) -> void;
		};

		struct random_seed_unlatch : non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		friend agent;
		explicit host(
			std::shared_ptr<storage> agent_storage,
			agent::clock::any_clock clock,
			std::optional<double> random_seed
		);

		auto clock_time_ms() -> int64_t;
		auto execute(const std::stop_token& stop_token) -> void;
		auto isolate() -> v8::Isolate*;
		auto random_seed_latch() -> std::experimental::scope_exit<random_seed_unlatch>;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;

	private:
		std::shared_ptr<storage> agent_storage;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator;
		std::unique_ptr<v8::Isolate, isolate_destructor> isolate_;
		v8::Global<v8::Context> scratch_context_;
		lockable<std::vector<task_type>, std::mutex, std::condition_variable_any> pending_tasks_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : non_moveable {
	public:
		explicit lock(host& agent_host);

		auto operator*() -> host& { return agent_host; }
		auto operator*() const -> const host& { return agent_host; }
		auto operator->() -> host* { return &agent_host; }
		auto operator->() const -> const host* { return &agent_host; }

	private:
		host& agent_host;
};

// This is constructed before (and may outlive) an agent
class agent::storage : non_moveable {
	public:
		explicit storage(scheduler& scheduler);

		auto scheduler_handle() -> scheduler::handle&;

	private:
		scheduler::handle scheduler_handle_;
};

} // namespace ivm
