module;
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
module ivm.isolated_v8;
import :agent;
import :platform.foreground_runner;
import :scheduler;
import v8;
import ivm.iv8;
import ivm.js;
import ivm.utility;

namespace ivm {

// lock
thread_local agent::lock* current_lock{};

agent::lock::lock(agent::host& host) :
		host_{host} {}

// managed_lock
agent::managed_lock::managed_lock(host& host) :
		lock{host},
		locker_{host.isolate()},
		scope_{host.isolate()} {}

// handle
agent::agent(
	const std::shared_ptr<host>& host,
	const std::shared_ptr<foreground_runner>& task_runner
) :
		host_{host},
		task_runner_{task_runner} {}

// storage
agent::storage::storage(scheduler& scheduler) :
		scheduler_handle_{scheduler} {}

auto agent::storage::scheduler_handle() -> scheduler::handle& {
	return scheduler_handle_;
}

// host
agent::host::host(
	std::shared_ptr<storage> agent_storage,
	std::shared_ptr<foreground_runner> task_runner,
	clock::any_clock clock,
	std::optional<double> random_seed
) :
		agent_storage_{std::move(agent_storage)},
		task_runner_{std::move(task_runner)},
		array_buffer_allocator_{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()},
		random_seed_{random_seed},
		clock_{clock} {
	isolate_->SetData(0, this);
	auto create_params = v8::Isolate::CreateParams{};
	create_params.array_buffer_allocator = array_buffer_allocator_.get();
	v8::Isolate::Initialize(isolate_.get(), create_params);
}

auto agent::host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) { return clock.clock_time_ms(); }, clock_);
}

auto agent::host::execute(std::stop_token stop_token) -> void {
	// Enter task runner scope
	task_runner_->scope([ & ](auto task_lock) {
		do {
			auto task = task_lock->acquire();
			if (task) {
				task_lock.unlock();
				run_task([ & ](auto& /*agent_lock*/) {
					std::visit([](auto& clock) { clock.begin_tick(); }, clock_);
					task->Run();
				});
				task_lock.lock();
			}
		} while (task_lock.wait(stop_token));
	});
}

auto agent::host::isolate() -> v8::Isolate* {
	return isolate_.get();
}

// v8 uses the same entropy source for `Math.random()` and also memory page randomization. We want
// to control the `Math.random()` seed without giving up memory randomness. Anyway it seems like the
// generator is initialized on context creation, so we just control the randomness in that one case.
auto agent::host::random_seed_latch() -> util::scope_exit<random_seed_unlatch> {
	should_give_seed_ = true;
	return util::scope_exit{random_seed_unlatch{should_give_seed_}};
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

auto agent::host::task_runner(v8::TaskPriority /*priority*/) -> std::shared_ptr<v8::TaskRunner> {
	return task_runner_;
}

auto agent::host::weak_module_specifiers() -> js::iv8::weak_map<v8::Module, js::string_t>& {
	return weak_module_specifiers_;
}

// isolate_destructor
auto agent::host::isolate_destructor::operator()(v8::Isolate* isolate) -> void {
	isolate->Dispose();
}

// random_seed_unlatch
agent::host::random_seed_unlatch::random_seed_unlatch(bool& latch) :
		latch{&latch} {};

auto agent::host::random_seed_unlatch::operator()() const -> void {
	*latch = false;
}

} // namespace ivm
