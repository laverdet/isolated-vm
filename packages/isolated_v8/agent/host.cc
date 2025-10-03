module;
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
module isolated_v8;
import :foreground_runner;
import :remote_handle;
import :scheduler;
import ivm.utility;
import v8_js;

namespace isolated_v8 {

// agent_severable
agent_severable::agent_severable(std::shared_ptr<agent_host> host) :
		host_{std::move(host)} {}

auto agent_severable::sever() -> void {
	host_.store(nullptr, std::memory_order_relaxed);
}

// agent_host
agent_host::agent_host(
	scheduler::layer<{}>& cluster_scheduler,
	std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
	behavior_params params
) :
		foreground_runner_{std::move(foreground_runner)},
		async_scheduler_{cluster_scheduler},
		array_buffer_allocator_{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()},
		remote_expiration_callback_{[ this ](expired_remote_type remote) { reset_remote_handle(std::move(remote)); }},
		random_seed_{params.random_seed},
		clock_{params.clock} {
	isolate_->SetData(0, this);
	auto create_params = v8::Isolate::CreateParams{};
	create_params.array_buffer_allocator = array_buffer_allocator_.get();
	v8::Isolate::Initialize(isolate_.get(), create_params);
}

auto agent_host::acquire_severable(const std::shared_ptr<agent_host>& self) -> std::shared_ptr<agent_severable> {
	auto severable = self->severable_.lock();
	if (!severable) {
		severable = std::make_shared<agent_severable>(self);
		self->severable_ = std::weak_ptr{severable};
	}
	return severable;
}

auto agent_host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) -> int64_t { return clock.clock_time_ms(); }, clock_);
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

auto agent_host::remote_handle_lock() -> isolated_v8::remote_handle_lock {
	return isolated_v8::remote_handle_lock{
		remote_handle_list_,
		std::shared_ptr<reset_handle_type>{shared_from_this(), &remote_expiration_callback_},
	};
}

auto agent_host::reset_remote_handle(expired_remote_type remote) -> void {
	foreground_runner::schedule_handle_task(
		foreground_runner_,
		[ this, remote = std::move(remote) ](const std::stop_token& /*stop_token*/) {
			auto lock = js::iv8::isolate_execution_lock{isolate()};
			remote->reset(lock);
			remote_handle_list_.erase(*remote);
		}
	);
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

} // namespace isolated_v8
