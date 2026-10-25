#include "promise_timeout.h"
#include "environment.h"
#include "lib/timer.h"
#include "platform_delegate.h"
#include "scheduler.h"
#include <algorithm>
#include <utility>

namespace ivm {

PromiseTimeout::PromiseTimeout(IsolateEnvironment& env, uint32_t timeout_ms) :
	env{env},
	weak_env{env.holder.lock()->GetIsolate()},
	default_env{Executor::GetDefaultEnvironment().holder.lock()->GetIsolate()},
	default_task_runner{PlatformDelegate::GetForegroundTaskRunner(Executor::GetDefaultEnvironment().GetIsolate())},
	deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds{timeout_ms}} {}

PromiseTimeout::~PromiseTimeout() {
	ReleaseUvRef();
}

void PromiseTimeout::UpdateUvRef(const std::weak_ptr<IsolateEnvironment>& default_env, bool acquire) {
	if (auto env = default_env.lock()) {
		auto& scheduler = static_cast<UvScheduler&>(*env->scheduler);
		if (acquire) {
			scheduler.IncrementUvRef();
		} else {
			scheduler.DecrementUvRef();
		}
	}
}

void PromiseTimeout::PostUvRef(bool acquire) {
	class UvRefTask final : public Runnable {
		public:
			UvRefTask(std::weak_ptr<IsolateEnvironment> default_env, bool acquire) : default_env{std::move(default_env)}, acquire{acquire} {}
			void Run() final { PromiseTimeout::UpdateUvRef(default_env, acquire); }
		private:
			std::weak_ptr<IsolateEnvironment> default_env;
			bool acquire;
	};
	if (auto runner = default_task_runner.lock()) {
		runner->PostTask(std::make_unique<UvRefTask>(default_env, acquire));
	}
}

void PromiseTimeout::PromiseCreated() {
	bool retain = false;
	{
		std::lock_guard lock{mutex};
		++pending_promises;
		if (!expired && !has_uv_ref) {
			has_uv_ref = true;
			retain = true;
		}
	}
	if (retain) {
		PostUvRef(true);
	}
	StartTimer();
}

void PromiseTimeout::PromiseResolved() {
	{
		std::lock_guard lock{mutex};
		assert(pending_promises != 0);
		--pending_promises;
	}
	StopIfUnused();
}

void PromiseTimeout::Watch(const void* owner, std::function<void()> callback) {
	bool retain = false;
	bool notify = false;
	{
		std::lock_guard lock{mutex};
		watchers.insert_or_assign(owner, std::move(callback));
		if (!expired && !has_uv_ref) {
			has_uv_ref = true;
			retain = true;
		}
		notify = expired;
	}
	if (retain) {
		PostUvRef(true);
	}
	if (notify) {
		PostNotification(false);
	} else {
		StartTimer();
	}
}

void PromiseTimeout::Unwatch(const void* owner) {
	{
		std::lock_guard lock{mutex};
		watchers.erase(owner);
	}
	StopIfUnused();
}

auto PromiseTimeout::IsExpired() -> bool {
	std::lock_guard lock{mutex};
	return expired;
}

void PromiseTimeout::StartTimer() {
	std::lock_guard lock{mutex};
	if (timer || expired || (pending_promises == 0 && watchers.empty())) {
		return;
	}
	auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
	std::weak_ptr<PromiseTimeout> weak = shared_from_this();
	timer = std::make_unique<timer_t>(static_cast<uint32_t>(std::max<int64_t>(remaining, 0)), [weak](void* next) {
		if (auto timeout = weak.lock()) {
			timeout->Expire();
		}
		timer_t::chain(next);
	});
}

void PromiseTimeout::StopIfUnused() {
	std::unique_ptr<timer_t> timer_to_stop;
	{
		std::lock_guard lock{mutex};
		if ((!expired && pending_promises != 0) || !watchers.empty()) {
			return;
		}
		timer_to_stop = std::move(timer);
	}
	timer_to_stop.reset();
	ReleaseUvRef();
}

void PromiseTimeout::ReleaseUvRef() {
	bool release = false;
	{
		std::lock_guard lock{mutex};
		if (!has_uv_ref || (!expired && pending_promises != 0) || !watchers.empty()) {
			return;
		}
		has_uv_ref = false;
		release = true;
	}
	if (release) {
		PostUvRef(false);
	}
}

void PromiseTimeout::PostNotification(bool interrupt) {
	auto env = weak_env.lock();
	if (!env || env->terminated) {
		return;
	}
	class Notification final : public Runnable {
		public:
			explicit Notification(std::shared_ptr<PromiseTimeout> timeout, bool interrupt) : timeout{std::move(timeout)}, interrupt{interrupt} {}
			void Run() final { timeout->RunNotification(interrupt); }
		private:
			std::shared_ptr<PromiseTimeout> timeout;
			bool interrupt;
	};
	if (interrupt) {
		auto lock = env->scheduler->Lock();
		if (on_default_thread) {
			lock->sync_interrupts.push(std::make_unique<Notification>(shared_from_this(), true));
			lock->InterruptSyncIsolate();
		} else {
			lock->interrupts.push(std::make_unique<Notification>(shared_from_this(), true));
			lock->InterruptIsolate();
		}
	} else {
		class WakeTask final : public Runnable {
			public:
				explicit WakeTask(std::weak_ptr<IsolateEnvironment> env) : weak_env{std::move(env)} {}
				void Run() final {
					if (auto env = weak_env.lock(); env && !env->terminated) {
						env->GetScheduler().Lock()->WakeIsolate(std::move(env));
					}
				}
			private:
				std::weak_ptr<IsolateEnvironment> weak_env;
		};
		env->scheduler->Lock()->tasks.push(std::make_unique<Notification>(shared_from_this(), false));
		if (auto runner = default_task_runner.lock()) {
			runner->PostTask(std::make_unique<WakeTask>(weak_env));
		}
	}
}

void PromiseTimeout::Expire() {
	bool interrupt = false;
	bool notify = false;
	{
		std::lock_guard lock{mutex};
		if (expired) {
			return;
		}
		expired = true;
		interrupt = reaction_depth != 0;
		notify = !watchers.empty();
	}
	if (interrupt || notify) {
		PostNotification(interrupt);
	} else {
		ReleaseUvRef();
	}
}

void PromiseTimeout::RunNotification(bool interrupt) {
	if (env.terminated) {
		return;
	}
	if (interrupt) {
		bool terminate = false;
		{
			std::lock_guard lock{mutex};
			if (reaction_depth != 0 && !did_terminate) {
				did_terminate = true;
				terminate = true;
			}
		}
		if (terminate) {
			++env.terminate_depth;
			env.GetIsolate()->TerminateExecution();
		}
	}
	v8::Context::Scope context_scope{env.DefaultContext()};
	Notify();
}

void PromiseTimeout::Notify() {
	std::unordered_map<const void*, std::function<void()>> callbacks;
	{
		std::lock_guard lock{mutex};
		callbacks = std::exchange(watchers, {});
	}
	for (const auto& [owner, callback] : callbacks) {
		callback();
	}
	StopIfUnused();
}

void PromiseTimeout::BeforeReaction() {
	bool terminate = false;
	bool notify = false;
	{
		std::lock_guard lock{mutex};
		++reaction_depth;
		on_default_thread = Executor::IsDefaultThread();
		if (!expired && std::chrono::steady_clock::now() >= deadline) {
			expired = true;
			notify = !watchers.empty();
		}
		if (expired && !did_terminate) {
			did_terminate = true;
			terminate = true;
		}
	}
	if (terminate) {
		++env.terminate_depth;
		env.GetIsolate()->TerminateExecution();
	}
	if (notify) {
		PostNotification(false);
	} else {
		ReleaseUvRef();
	}
}

void PromiseTimeout::AfterReaction() {
	bool cancel = false;
	{
		std::lock_guard lock{mutex};
		assert(reaction_depth != 0);
		--reaction_depth;
		if (reaction_depth == 0 && did_terminate) {
			did_terminate = false;
			cancel = true;
		}
	}
	if (cancel) {
		assert(env.terminate_depth > 0);
		if (--env.terminate_depth == 0) {
			env.GetIsolate()->CancelTerminateExecution();
		}
	}
}

PromiseTimeoutScope::PromiseTimeoutScope(IsolateEnvironment& env, uint32_t timeout_ms) : env{env} {
	if (!env.nodejs_isolate) {
		engaged = true;
		previous = std::exchange(env.current_promise_timeout, {});
		if (timeout_ms != 0) {
			current = std::make_shared<PromiseTimeout>(env, timeout_ms);
			env.current_promise_timeout = current;
		}
	}
}

PromiseTimeoutScope::~PromiseTimeoutScope() {
	if (engaged) {
		env.current_promise_timeout = previous;
	}
}

PromiseTimeoutReference::PromiseTimeoutReference(std::shared_ptr<PromiseTimeout> timeout) : timeout{std::move(timeout)} {
	this->timeout->PromiseCreated();
}

PromiseTimeoutReference::~PromiseTimeoutReference() {
	Resolve();
}

void PromiseTimeoutReference::Resolve() {
	if (!std::exchange(resolved, true)) {
		timeout->PromiseResolved();
	}
}

} // namespace ivm
