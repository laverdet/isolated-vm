module;
#include <memory>
#include <optional>
#include <utility>
#include <variant>
module ivm.isolated_v8;
import :agent;
import :platform;
import :platform.foreground_runner;
import :scheduler;
import v8;
import ivm.iv8;
import ivm.js;
import ivm.utility;

namespace ivm {

agent::lock::lock(agent::host& host) :
		host_{host} {}

// managed_lock
agent::managed_lock::managed_lock(host& host) :
		lock{host},
		locker_{host.isolate()},
		scope_{host.isolate()} {}

// handle
agent::agent(const std::shared_ptr<host>& host, std::shared_ptr<severable> severable_) :
		host_{host},
		severable_{std::move(severable_)} {}

// storage
agent::storage::storage(ivm::cluster_scheduler& cluster_scheduler) :
		scheduler_{cluster_scheduler} {}

// severable
agent::severable::severable(std::shared_ptr<host> host) :
		host_{std::move(host)} {}

auto agent::severable::sever() -> void {
	host_->severable_.reset();
}

// host
agent::host::host(
	std::shared_ptr<storage> agent_storage,
	clock::any_clock clock,
	std::optional<double> random_seed
) :
		agent_storage_{std::move(agent_storage)},
		task_runner_{std::make_shared<ivm::foreground_runner>(agent_storage_->scheduler())},
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

auto agent::host::foreground_runner() -> std::shared_ptr<ivm::foreground_runner> {
	return task_runner_;
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
