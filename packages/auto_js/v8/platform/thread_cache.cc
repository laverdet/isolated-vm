module;
#include <cassert>
module v8_js;
import :platform.thread_cache;
import std;

namespace js::iv8::platform {

// `thread_suspension`
thread_suspension::thread_suspension(runner_thread& thread) : thread_{thread} {
	thread.cache_.get().suspend(*this);
}

thread_suspension::~thread_suspension() {
	thread_.get().cache_.get().resume(*this);
}

// `runner_thread`
runner_thread::runner_thread(thread_cache& cache) :
		cache_{cache},
		thread_{[ this ] -> void {
			run();
		}} {}

runner_thread::~runner_thread() {
	semaphore_.release();
	thread_.join();
}

auto runner_thread::assign(job_type work) -> void {
	work_ = std::move(work);
	pending_.store(true, std::memory_order_release);
	semaphore_.release();
}

auto runner_thread::run() -> void {
	while (true) {
		semaphore_.acquire();
		auto work = std::exchange(work_, {});
		pending_.store(false, std::memory_order_relaxed);
		if (!work) {
			return;
		}
		work(*this);
		if (!cache_.get().release(*this)) {
			return;
		}
	}
}

// `thread_cache`
thread_cache::thread_cache(unsigned concurrency) : concurrency_{concurrency} {}

thread_cache::~thread_cache() {
	auto lock = storage_.write_waitable(is_idle);
	lock.wait();
	assert(lock->suspended.empty());
}

auto thread_cache::request(request_kind kind, runner_thread::job_type work) -> std::thread::id {
	auto [ thread, reclaim ] = [ & ] -> std::tuple<runner_thread*, thread_suspension*> {
		auto lock = storage_.write();
		if (kind == request_kind::soft && lock->active >= concurrency_) {
			return {nullptr, nullptr};
		} else if (lock->active >= concurrency_ && !lock->suspended.empty()) {
			auto& suspension = lock->suspended.front();
			lock->suspended.pop_front();
			auto& thread = suspension.thread_.get();
			thread.assign(std::move(work));
			return {&thread, &suspension};
		} else {
			auto* thread = [ & ] -> runner_thread* {
				if (lock->cached.empty()) {
					auto& thread = lock->threads.emplace_back(*this);
					thread.self_ = std::prev(lock->threads.end());
					return &thread;
				} else {
					auto& thread = lock->cached.front();
					lock->cached.pop_front();
					return &thread;
				}
			}();
			++lock->active;
			thread->assign(std::move(work));
			return {thread, nullptr};
		}
	}();
	if (reclaim != nullptr) {
		// nb: `request_stop` may run callbacks so it should be invoked without the lock
		reclaim->stop_source_.request_stop();
		reclaim->stopped_.store(true, std::memory_order_release);
	}
	return thread == nullptr ? std::thread::id{} : thread->id();
}

auto thread_cache::release(runner_thread& thread) -> bool {
	if (thread.reassigned()) {
		return true;
	}
	auto lock = storage_.write_notify(is_idle);
	if (!lock->retired.empty()) {
		lock->retired.pop_front();
	}
	--lock->active;
	if (lock->threads.size() <= concurrency_) {
		lock->cached.push_front(thread);
		return true;
	} else {
		lock->retired.splice(lock->retired.begin(), lock->threads, thread.self_);
		return false;
	}
}

auto thread_cache::suspend(thread_suspension& suspension) -> void {
	auto lock = storage_.write();
	if (lock->active > concurrency_) {
		suspension.stop_source_.request_stop();
		suspension.stopped_.store(true, std::memory_order_release);
	} else {
		lock->suspended.push_back(suspension);
	}
}

auto thread_cache::resume(thread_suspension& suspension) -> void {
	{
		auto lock = storage_.write();
		if (suspension.list_hook_.is_linked()) {
			lock->suspended.erase(suspended_list_type::s_iterator_to(suspension));
			return;
		}
	}
	// Reclaimed by `request`, which is still stopping us
	while (!suspension.stopped_.load(std::memory_order_acquire)) {
		std::this_thread::yield();
	}
}

} // namespace js::iv8::platform
