module;
#include <uv.h>
#include <functional>
#include <utility>
#include <vector>
module ivm.node;
import ivm.utility;
import :uv_scheduler;

namespace ivm {

auto uv_scheduler::close() -> void {
	auto& storage = *async_;
	auto lock = storage->write();
	async_->close();
}

auto uv_scheduler::decrement_ref() -> void {
	if (--refs_ == 0) {
		uv_unref(async_->handle_base());
	}
}

auto uv_scheduler::increment_ref() -> void {
	if (++refs_ == 1) {
		uv_ref(async_->handle_base());
	}
}

auto uv_scheduler::open(uv_loop_t* loop) -> void {
	async_->open(uv_async_init, loop, [](uv_async_t* async) {
		auto& handle = *handle_type::unwrap(async);
		auto tasks = std::exchange(*handle->write(), {});
		for (auto& task : tasks) {
			task();
		}
	});
	uv_unref(async_->handle_base());
}

auto uv_scheduler::schedule(task_type task) -> void {
	auto& storage = *async_;
	auto lock = storage->write();
	if (uv_is_active(storage.handle_base()) != 0) {
		lock->push_back(std::move(task));
		uv_async_send(storage.handle());
	}
}

} // namespace ivm
