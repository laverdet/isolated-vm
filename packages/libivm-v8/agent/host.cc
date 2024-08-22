module;
#include <condition_variable>
#include <experimental/scope>
#include <memory>
#include <mutex>
#include <stop_token>
#include <utility>
export module ivm.isolated_v8:agent;
import :agent.fwd;
import :agent.clock;
import :agent.lock;
import v8;
import ivm.utility;

namespace ivm {

struct isolate_destructor {
		auto operator()(v8::Isolate* isolate) -> void;
};

class agent::host : non_moveable {
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
		auto random_seed_latch();
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

agent::host::host(
	std::shared_ptr<storage> agent_storage,
	agent::clock::any_clock clock,
	std::optional<double> random_seed
) :
		agent_storage{std::move(agent_storage)},
		array_buffer_allocator{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()},
		random_seed_{random_seed},
		clock_{clock} {
	isolate_->SetData(0, this);
	auto create_params = v8::Isolate::CreateParams{};
	create_params.array_buffer_allocator = array_buffer_allocator.get();
	v8::Isolate::Initialize(isolate_.get(), create_params);
}

auto agent::host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) { return clock.clock_time_ms(); }, clock_);
}

auto agent::host::execute(const std::stop_token& stop_token) -> void {
	// Enter isolate on this thread
	const auto locker = v8::Locker{isolate_.get()};
	const auto isolate_scope = v8::Isolate::Scope{isolate_.get()};
	// Exclusive lock on tasks
	auto lock = pending_tasks_.write_waitable([](const std::vector<task_type>& tasks) {
		return !tasks.empty();
	});
	do {
		// Accept task list locally, then unlock
		auto tasks = std::move(*lock);
		lock.unlock();
		// Dispatch all tasks
		auto agent_lock = agent::lock{*this};
		for (auto& task : tasks) {
			const auto handle_scope = v8::HandleScope{isolate_.get()};
			std::visit([](auto& clock) { clock.begin_tick(); }, clock_);
			take(std::move(task))(agent_lock);
		}
		// Unlock v8 isolate before suspendiconst ng
		const auto unlocker = v8::Unlocker{isolate_.get()};
		// Wait for more tasks
		lock.lock();
	} while (lock.wait(stop_token));
}

auto agent::host::isolate() -> v8::Isolate* {
	return isolate_.get();
}

// v8 uses the same entropy source for `Math.random()` and also memory page randomization. We want
// to control the `Math.random()` seed without giving up memory randomness. Anyway it seems like the
// generator is initialized on context creation, so we just control the randomness in that one case.
auto agent::host::random_seed_latch() {
	should_give_seed_ = true;
	return std::experimental::scope_exit([ this ] {
		should_give_seed_ = false;
	});
}

auto agent::host::scratch_context() -> v8::Local<v8::Context> {
	if (scratch_context_.IsEmpty()) {
		auto latch = random_seed_latch();
		auto context = v8::Context::New(isolate_.get());
		scratch_context_.Reset(isolate_.get(), context);
		scratch_context_.SetWeak();
		return context;
	} else {
		return scratch_context_.Get(isolate_.get());
	}
}

auto agent::agent::schedule_task(task_type task) -> void {
	// auto agent_host::schedule_tasks(const std::shared_ptr<agent_host>& host, std::ranges::range auto tasks) -> void {
	// 	auto locked = pending_tasks.write_notify();
	// 	locked->tasks.reserve(locked->tasks.size() + std::ranges::size(tasks));
	// 	std::ranges::move(tasks, std::back_inserter(locked->tasks));
	// 	locked->host = host;
	// }
	auto host = this->host_.lock();
	if (host) {
		host->pending_tasks_.write_notify()->push_back(std::move(task));
	}
}

auto agent::host::get_current() -> host* {
	auto* isolate = v8::Isolate::TryGetCurrent();
	if (isolate == nullptr) {
		return nullptr;
	} else {
		return &get_current(isolate);
	}
}

auto agent::host::get_current(v8::Isolate* isolate) -> host& {
	return *static_cast<host*>(isolate->GetData(0));
}

auto agent::host::take_random_seed() -> std::optional<double> {
	if (should_give_seed_) {
		return std::exchange(random_seed_, std::nullopt);
	} else {
		return std::nullopt;
	}
}

auto isolate_destructor::operator()(v8::Isolate* isolate) -> void {
	isolate->Dispose();
}

} // namespace ivm
