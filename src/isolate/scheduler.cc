#include "environment.h"
#include "executor.h"
#include "node_wrapper.h"
#include "scheduler.h"
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <v8.h>
#include <utility>

using namespace v8;
namespace ivm {
namespace {

auto ceil_div(long long quota, long long period) -> unsigned {
	if (quota <= 0 || period <= 0) {
		return 0;
	}
	return static_cast<unsigned>((quota + period - 1) / period);
}

// Returns the CPU limit imposed on this process by its cgroup, or 0 if there is none.
auto cgroup_cpu_limit() -> unsigned {
#ifdef __linux__
	{
		// cgroup v2 writes "<quota> <period>", or "max <period>" when unconstrained.
		std::ifstream file{"/sys/fs/cgroup/cpu.max"};
		std::string quota;
		long long period = 0;
		if (file >> quota >> period && quota != "max") {
			char* end = nullptr;
			long long value = std::strtoll(quota.c_str(), &end, 10);
			if (end != quota.c_str() && *end == '\0') {
				return ceil_div(value, period);
			}
		}
	}
	{
		// cgroup v1 splits it across two files, and uses -1 for "unconstrained".
		std::ifstream quota_file{"/sys/fs/cgroup/cpu/cpu.cfs_quota_us"};
		std::ifstream period_file{"/sys/fs/cgroup/cpu/cpu.cfs_period_us"};
		long long quota = 0;
		long long period = 0;
		if (quota_file >> quota && period_file >> period) {
			return ceil_div(quota, period);
		}
	}
#endif
	return 0;
}

// `hardware_concurrency` reports the host's CPU count. Under a container orchestrator that is
// routinely far larger than the CPU time this process may actually use, and the surplus threads
// can never run concurrently -- they only cost an OS thread each, plus, on glibc, a malloc arena
// each. Size the pool from the cgroup quota when that is the smaller of the two.
auto thread_pool_size() -> size_t {
	unsigned threads = std::thread::hardware_concurrency();
	if (threads == 0) {
		threads = 1;
	}
	if (const char* env = std::getenv("ISOLATED_VM_THREAD_POOL_SIZE")) {
		char* end = nullptr;
		long long value = std::strtoll(env, &end, 10);
		if (end != env && *end == '\0' && value > 0) {
			return static_cast<size_t>(value);
		}
	}
	unsigned limit = cgroup_cpu_limit();
	if (limit > 0 && limit < threads) {
		threads = limit;
	}
	return static_cast<size_t>(threads) + 1;
}

thread_pool_t thread_pool{thread_pool_size()};
}

/*
 * Scheduler implementation
 */
void Scheduler::CancelAsync() {
	if (async_wait != nullptr) {
		async_wait->Cancel();
	}
}

void Scheduler::DoneRunning() {
	assert(status == Status::Running);
	status = Status::Waiting;
}

void Scheduler::InterruptIsolate() {
	assert(status == Status::Running);
	// Since this callback will be called by v8 we can be certain the pointer to `isolate` is still valid
	env.GetIsolate()->RequestInterrupt([](Isolate* /*isolate_ptr*/, void* env_ptr) {
		static_cast<IsolateEnvironment*>(env_ptr)->InterruptEntryAsync();
	}, &env);
}

void Scheduler::InterruptSyncIsolate() {
	env.GetIsolate()->RequestInterrupt([](Isolate* /*isolate_ptr*/, void* env_ptr) {
		static_cast<IsolateEnvironment*>(env_ptr)->InterruptEntrySync();
	}, &env);
}

auto Scheduler::WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate_ptr) -> bool {
	if (status == Status::Waiting) {
		status = Status::Running;
		// Move shared reference to this scheduler to ensure the IsolateEnvironment won't be deleted
		// before a thread picks up this work.
		assert(!env_ref);
		env_ref = std::move(isolate_ptr);
		IncrementUvRef();
		SendWake();
		return true;
	} else {
		return false;
	}
}

auto LockedScheduler::GetForegroundTaskRunner() -> std::shared_ptr<v8::TaskRunner> {
	return env.GetTaskRunner();
}

void LockedScheduler::DecrementUvRefForIsolate(const std::shared_ptr<IsolateHolder>& holder) {
	auto ref = holder->GetIsolate();
	if (ref) {
		ref->scheduler->DecrementUvRef();
	}
}

void LockedScheduler::IncrementUvRefForIsolate(const std::shared_ptr<IsolateHolder>& holder) {
	auto ref = holder->GetIsolate();
	if (ref) {
		ref->scheduler->IncrementUvRef();
	}
}

IsolatedScheduler::IsolatedScheduler(IsolateEnvironment& env, UvScheduler& default_scheduler) :
	LockedScheduler{env},
	default_scheduler{default_scheduler} {}

void IsolatedScheduler::DecrementUvRef() {
	default_scheduler.DecrementUvRef();
}

void IsolatedScheduler::IncrementUvRef() {
	default_scheduler.IncrementUvRef();
}

void IsolatedScheduler::SendWake() {
	thread_pool.exec(thread_affinity, [](bool pool_thread, void* param) {
		auto& scheduler = *static_cast<IsolatedScheduler*>(param);
		auto ref = std::exchange(scheduler.env_ref, {});
		ref->AsyncEntry();
		if (!pool_thread) {
			ref->GetIsolate()->DiscardThreadSpecificMetadata();
		}
		// Grab reference to default scheduler, since resetting `ref` may deallocate `scheduler` and
		// invalidate the instance. Resetting `ref` must take place here because the destructor might
		// invoke cleanup tasks on the default isolate which will increment `uv_ref_count`.
		// `uv_ref_count` needs to be incremented before it's decremented otherwise `UvScheduler` will
		// try to invoke `uv_ref` from a non-default thread.
		auto& default_scheduler = scheduler.default_scheduler;
		ref = {};
		if (--default_scheduler.uv_ref_count == 0) {
			// Wake up the libuv loop so we can unref the async handle from the default thread.
			uv_async_send(default_scheduler.uv_async);
		}
	}, this);
}

UvScheduler::UvScheduler(IsolateEnvironment& env) :
		LockedScheduler{env},
		loop{node::GetCurrentEventLoop(v8::Isolate::GetCurrent())},
		uv_async{new uv_async_t} {
	uv_async_init(loop, uv_async, [](uv_async_t* async) {
		auto& scheduler = *static_cast<UvScheduler*>(async->data);
		auto ref = [&]() {
			// Lock is required to access env_ref on the default scheduler but a non-default scheduler
			// doesn't need it. This is because `WakeIsolate` can trigger this function via
			// `uv_async_send` while another thread is writing to `env_ref`
			std::lock_guard<std::mutex> lock{scheduler.mutex};
			return std::exchange(scheduler.env_ref, {});
		}();
		if (ref) {
			ref->AsyncEntry();
			if (--scheduler.uv_ref_count == 0) {
				uv_unref(reinterpret_cast<uv_handle_t*>(scheduler.uv_async));
			}
		} else {
			if (scheduler.uv_ref_count.load() == 0) {
				uv_unref(reinterpret_cast<uv_handle_t*>(scheduler.uv_async));
			}
		}
	});
	uv_async->data = this;
	uv_unref(reinterpret_cast<uv_handle_t*>(uv_async));
}

UvScheduler::~UvScheduler() {
	uv_close(reinterpret_cast<uv_handle_t*>(uv_async), [](uv_handle_t* handle) {
		delete reinterpret_cast<uv_async_t*>(handle);
	});
}

void UvScheduler::DecrementUvRef() {
	if (--uv_ref_count == 0) {
		if (Executor::IsDefaultThread()) {
			uv_unref(reinterpret_cast<uv_handle_t*>(uv_async));
		} else {
			uv_async_send(uv_async);
		}
	}
}

void UvScheduler::IncrementUvRef() {
	if (++uv_ref_count == 1) {
		// Only the default thread should be able to reach this branch
		assert(Executor::IsDefaultThread());
		uv_ref(reinterpret_cast<uv_handle_t*>(uv_async));
	}
}

void UvScheduler::SendWake() {
	uv_async_send(uv_async);
}

/*
 * Scheduler::AsyncWait implementation
 */
Scheduler::AsyncWait::~AsyncWait() {
	scheduler.Lock()->async_wait = nullptr;
}

void Scheduler::AsyncWait::Cancel() {
	*state.write() = canceled;
	state.notify_one();
}

void Scheduler::AsyncWait::Done() {
	*state.write() = finished;
	state.notify_one();
}

auto Scheduler::AsyncWait::Wait() const -> Scheduler::AsyncWait::State {
	auto lock = state.read<true>();
	while (*lock == pending) {
		lock.wait();
	}
	return *lock;
}

} // namespace ivm
