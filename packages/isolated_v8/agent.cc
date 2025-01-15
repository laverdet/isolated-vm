module;
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
module ivm.isolated_v8;
import :agent;
import :platform;
import isolated_v8.foreground_runner;
import isolated_v8.lock;
import isolated_v8.remote_handle;
import isolated_v8.remote_handle_list;
import isolated_v8.scheduler;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace isolated_v8 {

// agent::lock
agent::lock::lock(std::shared_ptr<agent::host> host) :
		isolate_scope_lock{host->isolate()},
		host_{std::move(host)} {}

auto agent::lock::accept_remote_handle(remote_handle& remote) noexcept -> void {
	host_->remote_handle_list().insert(remote);
}

auto agent::lock::remote_expiration_task() const -> reset_handle_type {
	auto self = std::weak_ptr{host_};
	return [ self = std::move(self) ](expired_remote_type remote) {
		if (auto host = self.lock()) {
			foreground_runner::schedule_handle_task(
				host->foreground_runner(),
				[ host = host.get(),
					remote = std::move(remote) ](
					const std::stop_token& /*stop_token*/
				) {
					isolate_scope_lock lock{host->isolate()};
					remote->reset(lock);
					host->remote_handle_list().erase(*remote);
				}
			);
		}
	};
}

// handle
agent::agent(const std::shared_ptr<host>& host, std::shared_ptr<severable> severable_) :
		host_{host},
		severable_{std::move(severable_)} {}

// severable
agent::severable::severable(std::shared_ptr<host> host) :
		host_{std::move(host)} {}

auto agent::severable::sever() -> void {
	host_->severable_.reset();
}

// host
agent::host::host(
	scheduler::layer<{}>& cluster_scheduler,
	std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
	clock::any_clock clock,
	std::optional<double> random_seed
) :
		foreground_runner_{std::move(foreground_runner)},
		async_scheduler_{cluster_scheduler},
		array_buffer_allocator_{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()},
		random_seed_{random_seed},
		clock_{clock} {
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

auto agent::host::dispose_isolate(v8::Isolate* isolate) -> void {
	isolate->Dispose();
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

auto agent::host::task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> {
	return foreground_runner::get_for_priority(foreground_runner_, priority);
}

// random_seed_unlatch
agent::host::random_seed_unlatch::random_seed_unlatch(bool& latch) :
		latch{&latch} {};

auto agent::host::random_seed_unlatch::operator()() const -> void {
	*latch = false;
}

} // namespace isolated_v8
