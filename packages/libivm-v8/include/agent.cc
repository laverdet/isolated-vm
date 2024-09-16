module;
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
export module ivm.isolated_v8:agent;
import :platform.clock;
import :platform.foreground_runner;
import :scheduler;
import ivm.utility;
import v8;

namespace ivm {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;
		class storage;

		explicit agent(
			const std::shared_ptr<host>& host,
			const std::shared_ptr<foreground_runner>& task_runner
		);

		auto schedule(std::invocable<lock&> auto task) -> void;

	private:
		std::weak_ptr<host> host_;
		std::shared_ptr<foreground_runner> task_runner_;
};

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : util::non_moveable {
	public:
		explicit lock(host& host);
		~lock();

		auto operator*() -> host& { return host_; }
		auto operator*() const -> const host& { return host_; }
		auto operator->() -> host* { return &host_; }
		auto operator->() const -> const host* { return &host_; }

		static auto expect() -> lock&;

	private:
		host& host_;
		lock* prev_;
};

// This is constructed before (and may outlive) an agent
class agent::storage : util::non_moveable {
	public:
		explicit storage(scheduler& scheduler);

		auto scheduler_handle() -> scheduler::handle&;

	private:
		scheduler::handle scheduler_handle_;
};

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
class agent::host : util::non_moveable {
	private:
		struct isolate_destructor {
				auto operator()(v8::Isolate* isolate) -> void;
		};

		struct random_seed_unlatch : util::non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		friend agent;

		explicit host(
			std::shared_ptr<storage> agent_storage,
			std::shared_ptr<foreground_runner> task_runner,
			clock::any_clock clock,
			std::optional<double> random_seed
		);

		auto clock_time_ms() -> int64_t;
		auto execute(const std::stop_token& stop_token) -> void;
		auto isolate() -> v8::Isolate*;
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;

	private:
		std::shared_ptr<storage> agent_storage_;
		std::shared_ptr<foreground_runner> task_runner_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, isolate_destructor> isolate_;
		v8::Global<v8::Context> scratch_context_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

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

auto agent::schedule(std::invocable<lock&> auto task) -> void {
	task_runner_->schedule_non_nestable(std::make_unique<task_of<decltype(task)>>(std::move(task)));
}

} // namespace ivm
