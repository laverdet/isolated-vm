module;
#include <cassert>
#include <functional>
#include <thread>
#include <utility>
#include <vector>
import ivm.utility;
import nodejs;
module napi_js.uv_scheduler;

namespace js::napi {

auto uv_scheduler::close() -> void {
	// nb: It's not safe to dereference `auto& async` like the functions because `.close()` deletes
	// the underlying memory and would cause a dangling reference. I think `uv_close` is probably
	// always async but I'm not sure of it.
	assert((*async_)->thread_id == std::this_thread::get_id());
	[[maybe_unused]] auto abandoned_tasks =
		std::invoke([ & ]() {
			assert((*async_)->thread_id == std::this_thread::get_id());
			auto lock = (*async_)->shared.write();
			lock->is_open = false;
			auto tasks = std::exchange(lock->tasks, {});
			async_->close();
			return tasks;
		});
}

auto uv_scheduler::decrement_ref() -> void {
	auto& async = *async_;
	assert(async->thread_id == std::this_thread::get_id());
	if (--async->refs == 0) {
		uv_unref(async.handle());
	}
}

auto uv_scheduler::increment_ref() -> void {
	auto& async = *async_;
	assert(async->thread_id == std::this_thread::get_id());
	if (++async->refs == 1) {
		uv_ref(async.handle());
	}
}

auto uv_scheduler::open(uv_loop_t* loop) -> void {
	auto& async = *async_;
	assert(async->thread_id == std::this_thread::get_id());
	async->shared.write()->is_open = true;
	async.open(uv_async_init, loop, [](uv_async_t* async_handle) {
		auto& async = *handle_type::unwrap(async_handle);
		auto tasks = std::exchange(async->shared.write()->tasks, {});
		for (auto& task : tasks) {
			task();
		}
	});
	uv_unref(async.handle());
}

auto uv_scheduler::operator()(task_type task) -> void {
	auto& async = *async_;
	auto shared = async->shared.write();
	if (shared->is_open) {
		shared->tasks.push_back(std::move(task));
		uv_async_send(async.handle());
	}
}

} // namespace js::napi
