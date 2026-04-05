export module napi_js:api.uv_scheduler;
import :api.uv_handle;
import std;
import util;

#if _LIBCPP_VERSION
// clang 22.1.0
// /usr/lib/llvm-22/bin/../include/c++/v1/__new/allocate.h:39:30: error: no matching function for call to 'operator new'
//    39 |     return static_cast<_Tp*>(__builtin_operator_new(__size, static_cast<align_val_t>(__align)));
export {
	using ::operator new;
}
#endif

namespace js::napi {

// Shareable libuv scheduler
export class uv_scheduler {
	private:
		using task_type = util::maybe_move_only_function<auto()->void>;

	public:
		auto close(util::function_ref<auto()->void> close_hook) -> void;
		auto decrement_ref() -> void;
		auto increment_ref() -> void;
		auto open(uv_loop_t* loop) -> void;
		auto operator()(auto task, auto&&... args) const -> void;
		explicit operator bool() const { return bool{async_}; }

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

// ---

auto uv_scheduler::operator()(auto task, auto&&... args) const -> void {
	auto& async = *async_;
	auto shared = async->shared.write();
	if (shared->is_open) {
		shared->tasks.push_back(
			[ task = std::move(task),
				... args = std::forward<decltype(args)>(args) ]() mutable -> void {
				task(std::move(args)...);
			}
		);
		uv_async_send(async.handle());
	}
}

} // namespace js::napi
