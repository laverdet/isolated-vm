module;
#include <expected>
#include <tuple>
#include <utility>
export module backend_napi_v8.utility;
import backend_napi_v8.environment;
import isolated_js;
import napi_js;
import ivm.utility;
import nodejs;

namespace backend_napi_v8 {
using namespace js;

export using expected_value = std::expected<napi_value, napi_value>;

export auto make_promise(environment& env, auto accept) {
	// Make nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	auto* promise = js::napi::invoke(napi_create_promise, napi_env{env}, &deferred);

	// Invoked in napi environment and resolves the deferred
	auto resolve =
		[ accept = std::move(accept),
			env_ptr = napi_env{env},
			deferred ](
			auto&&... args
		) mutable {
			auto& env = environment::get(env_ptr);
			js::napi::handle_scope scope{napi_env{env}};
			env.scheduler().decrement_ref();
			if (auto result = accept(env, std::forward<decltype(args)>(args)...)) {
				js::napi::invoke0(napi_resolve_deferred, napi_env{env}, deferred, std::move(result).value());
			} else {
				js::napi::invoke0(napi_reject_deferred, napi_env{env}, deferred, std::move(result).error());
			}
		};

	// Dispatcher can be invoked in any thread, and schedules the resolution on the node thread.
	auto scheduler = env.scheduler();
	scheduler.increment_ref();
	auto dispatch =
		[ resolve = std::move(resolve),
			scheduler = std::move(scheduler) ](
			auto&&... args
		) mutable {
			scheduler(
				util::make_indirect_moveable_function(
					[ resolve = std::move(resolve),
						... args = std::forward<decltype(args)>(args) ]() mutable {
						resolve(std::forward<decltype(args)>(args)...);
					}
				)
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

} // namespace backend_napi_v8
