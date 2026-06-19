module;
#include <cassert>
module v8_js;
import :isolated.agent;
import std;

namespace js::iv8::isolated {

// `agent_remote_handle_lock`
agent_remote_handle_lock::agent_remote_handle_lock(isolate_lock_witness lock, agent_host& agent) :
		remote_handle_lock{agent.make_remote_handle_lock(lock)} {}

// `agent_collected_handle_lock`
agent_collected_handle_lock::agent_collected_handle_lock(isolate_lock_witness lock, agent_host& agent) :
		collected_handle_lock{lock, agent.autorelease_pool()} {};

// `agent_storage`
// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto agent_storage::release(std::shared_ptr<agent_storage> storage) -> void {
	storage->cluster_.get().release_agent_storage(std::move(storage));
}

// `agent_handle`
agent_handle::agent_handle(std::shared_ptr<agent_host> host) :
		host_{host} {
	if (host && host->handle_count_.fetch_add(1, std::memory_order_relaxed) == 0) {
		host->self_handle_.store(std::move(host), std::memory_order_relaxed);
	}
}

agent_handle::agent_handle(const agent_handle& handle) :
		host_{handle.host_},
		weak_{handle.weak_} {
	if (!weak_) {
		if (auto host = host_.lock()) {
			host->handle_count_.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

agent_handle::~agent_handle() {
	if (!weak_) {
		if (auto host = host_.lock()) {
			if (
				host->handle_count_.load(std::memory_order_acquire) == 1 ||
				host->handle_count_.fetch_sub(1, std::memory_order_acq_rel) == 1
			) {
				host->self_handle_.store(nullptr, std::memory_order_relaxed);
			}
		}
	}
}

auto agent_handle::dispose(dispose_callback_type after_callback) -> void {
	if (auto host = host_.lock()) {
		assert(!host->dispose_callback_);
		host->dispose_callback_ = std::move(after_callback);
		host->executor_.terminate();
		host->self_handle_.store(nullptr, std::memory_order_release);
	}
}

auto agent_handle::lock_host() const -> std::shared_ptr<agent_host> {
	auto host = host_.lock();
	if (host && host->executor_.is_terminated()) {
		host.reset();
	}
	return host;
}

// `agent_host`
agent_host::agent_host(
	destroy_callback_type destroy_callback,
	std::shared_ptr<agent_storage> storage,
	behavior_params params
) :
		storage_{std::move(storage)},
		clock_{params.clock},
		destroy_callback_{destroy_callback},
		reset_handle_callback_{reset_handle_type{util::fn<&agent_host::remote_expiration_callback>, *this}},
		random_seed_{params.random_seed} {
	executor_.initialize([ & ](v8::Isolate* isolate) noexcept -> auto {
		isolate->SetData(0, this);
		auto create_params = v8::Isolate::CreateParams{};
		create_params.array_buffer_allocator_shared =
			std::shared_ptr<v8::ArrayBuffer::Allocator>(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
		return create_params;
	});
	executor_.isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
}

agent_host::~agent_host() {
	// Terminate foreground runner
	auto& scheduler = storage_->foreground_runner().scheduler();
	scheduler.terminate();
	if (!scheduler.is_this_thread()) {
		auto thread = scheduler.invoke([](auto& controller) -> auto {
			return controller.take();
		});
		thread.join();
	}
	// Destroy isolate and remaining handles
	{
		auto lock = js::iv8::isolate_destructor_lock{executor_.isolate()};
		scratch_context_.Reset();
		remote_handle_list_.clear(util::slice(lock));
		autorelease_pool_.clear();
		destroy_callback_(util::slice(lock));
	}
	// Deallocate isolate (before `agent_storage`)
	executor_.reset();
	// Release storage
	agent_storage::release(std::move(storage_));
	// Notify client
	if (dispose_callback_) {
		dispose_callback_();
	}
}

auto agent_host::make_context() -> v8::Local<v8::Context> {
	// v8 uses the same entropy source for `Math.random()` and also memory page randomization. We want
	// to control the `Math.random()` seed without giving up memory randomness. Anyway it seems like
	// the generator is initialized on context creation, so we just control the randomness in that one
	// case.
	should_give_seed_ = true;
	auto context = v8::Context::New(executor_.isolate());
	should_give_seed_ = false;
	return context;
}

auto agent_host::make_remote_handle_lock(isolate_lock_witness lock) -> remote_handle_lock {
	auto reset_handle = std::shared_ptr<reset_handle_type>{shared_from_this(), &reset_handle_callback_.at(0)};
	return remote_handle_lock{lock, remote_handle_list_, reset_handle};
}

auto agent_host::remote_expiration_callback(expired_remote_type remote) noexcept -> void {
	storage_->foreground_runner().schedule_handle_task(
		[ this, remote = std::move(remote) ](std::stop_token /*stop_token*/) -> void {
			auto lock = isolate_execution_lock{executor_.isolate()};
			remote->reset(util::slice(lock));
			remote_handle_list_.erase(*remote);
		}
	);
}

auto agent_host::scratch_context() -> v8::Local<v8::Context> {
	if (scratch_context_.IsEmpty()) {
		auto context = make_context();
		scratch_context_.Reset(executor_.isolate(), context);
		scratch_context_.SetWeak();
		return context;
	} else {
		return scratch_context_.Get(executor_.isolate());
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
	auto runner = std::shared_ptr<platform::foreground_runner>{storage_, &storage_->foreground_runner()};
	return platform::foreground_runner::get_for_priority(std::move(runner), priority);
}

auto agent_host::get_current(v8::Isolate* isolate) -> agent_host& {
	return *static_cast<agent_host*>(isolate->GetData(0));
}

} // namespace js::iv8::isolated
