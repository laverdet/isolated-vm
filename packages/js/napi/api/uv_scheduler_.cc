module;
#include <memory>
#include <thread>
export module napi_js:api.uv_scheduler;
import :api.uv_handle;
import ivm.utility;

namespace js::napi {

// Shareable libuv scheduler
export class uv_scheduler {
	private:
		using task_type = util::maybe_move_only_function<auto()->void>;

	public:
		auto close() -> void;
		auto decrement_ref() -> void;
		auto increment_ref() -> void;
		auto open(uv_loop_t* loop) -> void;
		auto operator()(task_type task) const -> void;

	private:
		struct locked_storage {
				std::vector<task_type> tasks;
				bool is_open{};
		};
		struct storage {
				storage() = default;
				util::lockable<locked_storage> shared;
				std::thread::id thread_id{std::this_thread::get_id()};
				int refs{};
		};
		using handle_type = uv_handle_of<uv_async_t, storage>;
		std::shared_ptr<handle_type> async_{handle_type::make()};
};

// An environment with an uv loop and scheduler
export class uv_schedulable {
	public:
		auto scheduler(this auto& self) -> auto& { return self.uv_scheduler_; }

	private:
		uv_scheduler uv_scheduler_;
};

} // namespace js::napi
