#include "environment.h"
#include "executor.h"
#include "scheduler.h"

using namespace v8;
namespace ivm {
namespace {

void AsyncCallbackCommon(bool pool_thread, void* param) {
	auto isolate_ptr_ptr = static_cast<std::shared_ptr<IsolateEnvironment>*>(param);
	auto isolate_ptr = std::shared_ptr<IsolateEnvironment>(std::move(*isolate_ptr_ptr));
	delete isolate_ptr_ptr;
	isolate_ptr->AsyncEntry();
	if (!pool_thread) {
		isolate_ptr->GetIsolate()->DiscardThreadSpecificMetadata();
	}
}

Scheduler* default_scheduler;
uv_async_t root_async;
thread_pool_t thread_pool{std::thread::hardware_concurrency() + 1};
std::atomic<int> uv_ref_count{0};

} // anonymous namespace

/*
 * Scheduler::Implementation implementation
 */
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
		IsolateEnvironment& isolate = *isolate_ptr;
		// Grab shared reference to this which will be passed to the worker entry. This ensures the
		// IsolateEnvironment won't be deleted before a thread picks up this work.
		auto isolate_ptr_ptr = new std::shared_ptr<IsolateEnvironment>(std::move(isolate_ptr));
		IncrementUvRef();
		if (isolate.root) {
			assert(root_async.data == nullptr);
			root_async.data = isolate_ptr_ptr;
			uv_async_send(&root_async);
		} else {
			thread_pool.exec(thread_affinity, [](bool pool_thread, void* param) {
				AsyncCallbackCommon(pool_thread, param);
				if (--uv_ref_count == 0) {
					// Wake up the libuv loop so we can unref the async handle from the default thread.
					uv_async_send(&root_async);
				}
			}, isolate_ptr_ptr);
		}
		return true;
	} else {
		return false;
	}
}

void Scheduler::Init(IsolateEnvironment& default_isolate) {
	default_scheduler = &default_isolate.scheduler;
	uv_async_init(uv_default_loop(), &root_async, [](uv_async_t* async) {
		// We need a lock on the default isolate's scheduler to access this data because it can be
		// modified from `WakeIsolate` while the non-default case (which doesn't acquire a lock) is
		// triggering the uv_async_send.
		void* data;
		{
			Scheduler::Lock lock{*default_scheduler};
			data = async->data;
			async->data = nullptr;
		}
		if (data == nullptr) {
			if (uv_ref_count.load() == 0) {
				uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
			}
		} else {
			AsyncCallbackCommon(true, data);
			if (--uv_ref_count == 0) {
				uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
			}
		}
	});
	root_async.data = nullptr;
	uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
}


void Scheduler::IncrementUvRef() {
	if (++uv_ref_count == 1) {
		// Only the default thread should be able to reach this branch
		assert(std::this_thread::get_id() == Executor::default_thread);
		uv_ref(reinterpret_cast<uv_handle_t*>(&root_async));
	}
}

void Scheduler::DecrementUvRef() {
	if (--uv_ref_count == 0) {
		if (std::this_thread::get_id() == Executor::default_thread) {
			uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		} else {
			uv_async_send(&root_async);
		}
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
	std::lock_guard<std::mutex> lock{scheduler.impl.wait_mutex};
	ready = true;
	if (done) {
		scheduler.impl.wait_cv.notify_one();
	}
}

void Scheduler::AsyncWait::Wait() {
	std::unique_lock<std::mutex> lock{scheduler.impl.wait_mutex};
	while (!ready || !done) {
		scheduler.impl.wait_cv.wait(lock);
	}
}

void Scheduler::AsyncWait::Wake() {
	std::lock_guard<std::mutex> lock{scheduler.impl.wait_mutex};
	done = true;
	if (ready) {
		scheduler.impl.wait_cv.notify_one();
	}
}

} // namespace ivm
