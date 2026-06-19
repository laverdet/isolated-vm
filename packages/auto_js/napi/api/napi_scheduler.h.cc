export module napi_js:api.napi_scheduler;
import :api.threadsafe_function;
import nodejs;
import std;
import util;

namespace js::napi {

// Shareable napi scheduler via napi_threadsafe_function
export class napi_scheduler {
	private:
		struct storage;
		using task_type = util::move_only_function<auto(napi_env, napi_value) noexcept -> void>;
		using threadsafe_function_type = threadsafe_function_of<storage, task_type>;

	public:
		napi_scheduler() = default;
		explicit napi_scheduler(napi_env env);
		auto operator()(auto task, auto&&... args) const noexcept -> bool;
		explicit operator bool() const noexcept;
		auto close(threadsafe_function_type::close_callback close) noexcept -> void;
		auto decrement_ref(node_api_basic_env env) const -> void;
		auto increment_ref(node_api_basic_env env) const -> void;

	private:
		struct storage {
				storage() = default;
				auto operator()(napi_env env, napi_value value, task_type& task) noexcept -> void;
				int refs{};
		};

		threadsafe_function_of<storage, task_type> fn_;
};

// An environment with an uv loop and scheduler
export class napi_schedulable {
	public:
		explicit napi_schedulable(napi_env env) : scheduler_{env} {}
		[[nodiscard]] auto scheduler(this auto& self) -> auto& { return self.scheduler_; }

	private:
		napi_scheduler scheduler_;
};

// ---

auto napi_scheduler::operator()(auto task, auto&&... args) const noexcept -> bool {
	return fn_(
		[ task = std::move(task),
			... args = std::forward<decltype(args)>(args) ](
			napi_env env,
			napi_value value
		) mutable -> void {
			task(env, value, std::move(args)...);
		}
	);
}

} // namespace js::napi
