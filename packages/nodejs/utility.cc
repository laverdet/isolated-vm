module;
#include <cstring>
#include <expected>
#include <tuple>
#include <utility>
export module ivm.node:utility;
import :environment;
import ivm.js;
import ivm.napi;
import ivm.utility;
import nodejs;

namespace ivm {

export using expected_value = std::expected<napi_value, napi_value>;

auto make_promise(environment& ienv, auto accept) {
	auto* env = ienv.nenv();

	// nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	auto* promise = js::napi::invoke(napi_create_promise, env, &deferred);

	// nodejs promise fulfillment
	ienv.scheduler().increment_ref();
	auto dispatch =
		[ accept = std::move(accept),
			deferred,
			env,
			scheduler = ienv.scheduler() ](
			auto&&... args
		) mutable {
			scheduler.schedule(
				[ accept = std::move(accept),
					deferred,
					env,
					... args = std::forward<decltype(args)>(args) ]() mutable {
					auto scope = js::napi::handle_scope{env};
					auto& ienv = environment::get(env);
					ienv.scheduler().decrement_ref();
					if (auto result = accept(ienv, std::forward<decltype(args)>(args)...)) {
						js::napi::invoke0(napi_resolve_deferred, env, deferred, result.value());
					} else {
						js::napi::invoke0(napi_reject_deferred, env, deferred, result.error());
					}
				}
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

} // namespace ivm
