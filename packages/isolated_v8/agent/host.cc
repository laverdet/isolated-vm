module;
#include <memory>
#include <optional>
#include <utility>
#include <variant>
module isolated_v8;
import :foreground_runner;
import :scheduler;
import ivm.utility;
import v8_js;

namespace isolated_v8 {

// agent_host
agent_host::agent_host(
	scheduler::layer<{}>& cluster_scheduler,
	std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
	agent::behavior_params params
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

agent_host::~agent_host() {
	foreground_runner_->close();
	js::iv8::isolate_managed_lock lock{isolate_.get()};
	autorelease_pool_.clear();
	remote_handle_list_.reset(lock);
	foreground_runner_->finalize();
}

auto agent_host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) { return clock.clock_time_ms(); }, clock_);
}

auto agent_host::make_handle(std::shared_ptr<agent_host> self) -> agent {
	auto severable = self->severable_.lock();
	if (!severable) {
		severable = std::make_shared<agent_severable>(self);
		self->severable_ = severable;
	}
	return agent{std::move(self), std::move(severable)};
}

// v8 uses the same entropy source for `Math.random()` and also memory page randomization. We want
// to control the `Math.random()` seed without giving up memory randomness. Anyway it seems like the
// generator is initialized on context creation, so we just control the randomness in that one case.
auto agent_host::random_seed_latch() -> util::scope_exit<random_seed_unlatch> {
	should_give_seed_ = true;
	return util::scope_exit{random_seed_unlatch{should_give_seed_}};
}

auto agent_host::scratch_context() -> v8::Local<v8::Context> {
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

auto agent_host::get_current() -> agent_host* {
	auto* isolate = v8::Isolate::TryGetCurrent();
	if (isolate == nullptr) {
		return nullptr;
	} else {
		return &get_current(isolate);
	}
}

auto agent_host::take_random_seed() -> std::optional<double> {
	if (should_give_seed_) {
		return std::exchange(random_seed_, std::nullopt);
	} else {
		return std::nullopt;
	}
}

auto agent_host::task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return foreground_runner::get_for_priority(foreground_runner_, priority);
}

// agent_host::random_seed_unlatch
agent_host::random_seed_unlatch::random_seed_unlatch(bool& latch) :
		latch{&latch} {};

auto agent_host::random_seed_unlatch::operator()() const -> void {
	*latch = false;
}

// agent_severable
agent_severable::agent_severable(std::shared_ptr<agent_host> host) :
		host_{std::move(host)} {}

auto agent_severable::sever() -> void {
	host_.store(nullptr, std::memory_order_relaxed);
}

} // namespace isolated_v8
