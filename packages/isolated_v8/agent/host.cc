module;
#include <memory>
#include <utility>
module isolated_v8.agent;
import isolated_v8.foreground_runner;
import isolated_v8.lock;
import isolated_v8.scheduler;
import ivm.utility;

namespace isolated_v8 {

// host
agent::host::host(
	scheduler::layer<{}>& cluster_scheduler,
	std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
	behavior_params params
) :
		foreground_runner_{std::move(foreground_runner)},
		async_scheduler_{cluster_scheduler},
		array_buffer_allocator_{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()},
		random_seed_{params.random_seed},
		clock_{params.clock} {
	isolate_->SetData(0, this);
	auto create_params = v8::Isolate::CreateParams{};
	create_params.array_buffer_allocator = array_buffer_allocator_.get();
	v8::Isolate::Initialize(isolate_.get(), create_params);
}

agent::host::~host() {
	foreground_runner_->close();
	isolate_scope_lock lock{isolate_.get()};
	remote_handle_list_.reset(lock);
	foreground_runner_->finalize();
}

auto agent::host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) { return clock.clock_time_ms(); }, clock_);
}

auto agent::host::make_handle(const std::shared_ptr<host>& self) -> agent {
	auto severable = self->severable_.lock();
	if (!severable) {
		severable = std::make_shared<agent::severable>(self);
		self->severable_ = severable;
	}
	return {self, std::move(severable)};
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

auto agent::host::take_random_seed() -> std::optional<double> {
	if (should_give_seed_) {
		return std::exchange(random_seed_, std::nullopt);
	} else {
		return std::nullopt;
	}
}

auto agent::host::task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return foreground_runner::get_for_priority(foreground_runner_, priority);
}

// agent::host::random_seed_unlatch
agent::host::random_seed_unlatch::random_seed_unlatch(bool& latch) :
		latch{&latch} {};

auto agent::host::random_seed_unlatch::operator()() const -> void {
	*latch = false;
}

} // namespace isolated_v8
