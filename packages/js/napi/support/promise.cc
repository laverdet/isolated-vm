module;
#include <concepts>
#include <tuple>
#include <utility>
export module napi_js:promise;
import :api;
import :handle_scope;
import ivm.utility;

namespace js::napi {

export template <class Environment, class Accept>
auto make_promise(Environment& env, Accept accept) {
	// Make nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_value promise;
	js::napi::invoke0(napi_create_promise, napi_env{env}, &deferred, &promise);

	// Invoked in napi environment and resolves the deferred
	auto resolve =
		[ accept = std::move(accept),
			env_ptr = napi_env{env},
			deferred ](
			auto&&... args
		) -> void {
		auto& env = Environment::unsafe_get(env_ptr);
		const auto scope = js::napi::handle_scope{napi_env{env}};
		env.scheduler().decrement_ref();
		try {
			auto result = js::transfer_in<napi_value>(accept(env, std::forward<decltype(args)>(args)...), env);
			js::napi::invoke0(napi_resolve_deferred, napi_env{env}, deferred, result);
		} catch (const napi::pending_error& /*error*/) {
			auto* exception = js::napi::invoke(napi_get_and_clear_last_exception, napi_env{env});
			js::napi::invoke0(napi_reject_deferred, napi_env{env}, deferred, exception);
		} catch (const js::error& error) {
			auto exception = js::transfer_in<napi_value>(accept(env, std::forward<decltype(args)>(args)...), env);
			js::napi::invoke0(napi_reject_deferred, napi_env{env}, deferred, exception);
		}
	};

	// Dispatcher can be invoked in any thread, and schedules the resolution on the node thread.
	auto scheduler = env.scheduler();
	scheduler.increment_ref();
	auto dispatch =
		[ resolve = std::move(resolve),
			scheduler = std::move(scheduler) ](
			auto&&... args
		) -> void
		requires std::invocable<Accept&, Environment&, decltype(args)...> {
			scheduler(
				util::make_indirect_moveable_function(
					[ resolve = std::move(resolve),
						... args = std::forward<decltype(args)>(args) ]() mutable -> void {
						resolve(std::forward<decltype(args)>(args)...);
					}
				)
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

} // namespace js::napi
