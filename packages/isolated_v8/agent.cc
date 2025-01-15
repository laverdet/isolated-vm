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
import isolated_v8.remote;
import isolated_v8.scheduler;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace isolated_v8 {

// agent::isolate_lock
agent::isolate_lock::isolate_lock(v8::Isolate* isolate) :
		locker_{isolate},
		scope_{isolate} {}

// agent::lock
agent::lock::lock(std::shared_ptr<agent::host> host) :
		agent::isolate_lock{host->isolate()},
		host_{std::move(host)} {}

auto agent::lock::isolate() -> v8::Isolate* {
	return host_->isolate();
}

auto agent::lock::remote_handle_delegate() -> std::weak_ptr<isolated_v8::remote_handle_delegate> {
	return std::static_pointer_cast<isolated_v8::remote_handle_delegate>(host_);
}

auto agent::lock::remote_handle_list() -> isolated_v8::remote_handle_list& {
	return host_->remote_handle_list();
}

// agent::termination_lock
agent::termination_lock::termination_lock(agent::host& host) :
		agent::isolate_lock{host.isolate()},
		host_{&host} {}

auto agent::termination_lock::isolate() -> v8::Isolate* {
	return host_->isolate();
}

auto agent::termination_lock::remote_handle_list() -> isolated_v8::remote_handle_list& {
	return host_->remote_handle_list();
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
	agent::termination_lock lock{*this};
	remote_handle_list_.reset(lock);
	foreground_runner_->finalize();
}

auto agent::host::clock_time_ms() -> int64_t {
	return std::visit([](auto&& clock) { return clock.clock_time_ms(); }, clock_);
}

auto agent::host::dispose_isolate(v8::Isolate* isolate) -> void {
	isolate->Dispose();
}

auto agent::host::foreground_runner() -> std::shared_ptr<isolated_v8::foreground_runner> {
	return foreground_runner_;
}

auto agent::host::isolate() -> v8::Isolate* {
	return isolate_.get();
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

auto agent::host::remote_handle_list() -> isolated_v8::remote_handle_list& {
	return remote_handle_list_;
}

auto agent::host::reset_remote_handle(remote_handle_delegate::expired_remote_type remote) -> void {
	foreground_runner::schedule_handle_task(
		foreground_runner_,
		[ this, remote = std::move(remote) ](const std::stop_token& /*stop_token*/) mutable {
			agent::termination_lock lock{*this};
			remote->reset(lock);
		}
	);
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

auto agent::host::weak_module_specifiers() -> js::iv8::weak_map<v8::Module, js::string_t>& {
	return weak_module_specifiers_;
}

// random_seed_unlatch
agent::host::random_seed_unlatch::random_seed_unlatch(bool& latch) :
		latch{&latch} {};

auto agent::host::random_seed_unlatch::operator()() const -> void {
	*latch = false;
}

} // namespace isolated_v8
