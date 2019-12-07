#include "environment.h"
#include "executor.h"
#include "node_wrapper.h"
#include "scheduler.h"
#include <memory>
#include <v8.h>
#include <utility>

using namespace v8;
namespace ivm {
namespace {
	thread_pool_t thread_pool{std::thread::hardware_concurrency() + 1};
}

/*
 * Scheduler::Implementation implementation
 */
Scheduler::Implementation::Implementation(IsolateEnvironment& env) :
		env{env},
		default_scheduler{[&]() -> Implementation& {
			auto env = Executor::GetCurrentEnvironment();
			return env == nullptr ? *this : env->scheduler.impl;
		}()} {
	if (this == &default_scheduler) {
		loop = node::GetCurrentEventLoop(v8::Isolate::GetCurrent());
		uv_async = new uv_async_t;
		uv_async_init(loop, uv_async, [](uv_async_t* async) {
			auto& scheduler = *static_cast<Scheduler::Implementation*>(async->data);
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
}

Scheduler::Implementation::~Implementation() {
	if (this == &default_scheduler) {
		uv_close(reinterpret_cast<uv_handle_t*>(uv_async), [](uv_handle_t* handle) {
			delete handle;
		});
	}
}

void Scheduler::Implementation::CancelAsync() {
	if (async_wait != nullptr) {
		async_wait->Wake();
	}
}

void Scheduler::Implementation::DoneRunning() {
	assert(status == Status::Running);
	status = Status::Waiting;
}

void Scheduler::Implementation::InterruptIsolate() {
	assert(status == Status::Running);
	// Since this callback will be called by v8 we can be certain the pointer to `isolate` is still valid
	env->RequestInterrupt([](Isolate* /* isolate_ptr */, void* env_ptr) {
		static_cast<IsolateEnvironment*>(env_ptr)->InterruptEntryAsync();
	}, &env);
}

void Scheduler::Implementation::InterruptSyncIsolate() {
	env->RequestInterrupt([](Isolate* /* isolate_ptr */, void* env_ptr) {
		static_cast<IsolateEnvironment*>(env_ptr)->InterruptEntrySync();
	}, &env);
}

auto Scheduler::Implementation::WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate_ptr) -> bool {
	if (status == Status::Waiting) {
		status = Status::Running;
		// Move shared reference to this scheduler to ensure the IsolateEnvironment won't be deleted
		// before a thread picks up this work.
		assert(!env_ref);
		env_ref = std::move(isolate_ptr);
		IncrementUvRef();
		if (this == &default_scheduler) {
			uv_async_send(uv_async);
		} else {
			thread_pool.exec(thread_affinity, [](bool pool_thread, void* param) {
				auto& scheduler = *static_cast<Scheduler::Implementation*>(param);
				auto ref = std::exchange(scheduler.env_ref, {});
				ref->AsyncEntry();
				if (!pool_thread) {
					ref->GetIsolate()->DiscardThreadSpecificMetadata();
				}
				if (--scheduler.default_scheduler.uv_ref_count == 0) {
					// Wake up the libuv loop so we can unref the async handle from the default thread.
					uv_async_send(scheduler.default_scheduler.uv_async);
				}
			}, this);
		}
		return true;
	} else {
		return false;
	}
}

void Scheduler::Implementation::IncrementUvRef() {
	if (++default_scheduler.uv_ref_count == 1) {
		// Only the default thread should be able to reach this branch
		assert(Executor::IsDefaultThread());
		uv_ref(reinterpret_cast<uv_handle_t*>(default_scheduler.uv_async));
	}
}

void Scheduler::Implementation::DecrementUvRef() {
	if (--default_scheduler.uv_ref_count == 0) {
		if (Executor::IsDefaultThread()) {
			uv_unref(reinterpret_cast<uv_handle_t*>(default_scheduler.uv_async));
		} else {
			uv_async_send(default_scheduler.uv_async);
		}
	}
}

/**
 * Scheduler (unlocked) implementation
 */
auto Scheduler::GetForegroundTaskRunner() -> std::shared_ptr<v8::TaskRunner> {
	return impl.env.GetTaskRunner();
}

void Scheduler::IncrementUvRef(const std::shared_ptr<IsolateHolder>& holder) {
	auto ref = holder->GetIsolate();
	if (ref) {
		ref->scheduler.impl.IncrementUvRef();
	}
}

void Scheduler::DecrementUvRef(const std::shared_ptr<IsolateHolder>& holder) {
	auto ref = holder->GetIsolate();
	if (ref) {
		ref->scheduler.impl.DecrementUvRef();
	}
}

/*
 * Scheduler::AsyncWait implementation
 */
Scheduler::AsyncWait::AsyncWait(Scheduler& scheduler) : scheduler{scheduler} {
	std::lock_guard<std::mutex> lock{scheduler.impl.mutex};
	scheduler.impl.async_wait = this;
}

Scheduler::AsyncWait::~AsyncWait() {
	std::lock_guard<std::mutex> lock{scheduler.impl.mutex};
	scheduler.impl.async_wait = nullptr;
}

void Scheduler::AsyncWait::Ready() {
	std::lock_guard<std::mutex> lock{scheduler.impl.mutex};
	ready = true;
	if (done) {
		scheduler.impl.cv.notify_one();
	}
}

void Scheduler::AsyncWait::Wait() {
	std::unique_lock<std::mutex> lock{scheduler.impl.mutex};
	while (!ready || !done) {
		scheduler.impl.cv.wait(lock);
	}
}

void Scheduler::AsyncWait::Wake() {
	std::lock_guard<std::mutex> lock{scheduler.impl.mutex};
	done = true;
	if (ready) {
		scheduler.impl.cv.notify_one();
	}
}

} // namespace ivm
