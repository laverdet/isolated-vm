module;
#include <cstring>
#include <expected>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.node:utility;
import :environment;
import ivm.napi;
import ivm.utility;
import ivm.value;
import napi;

namespace ivm {

export using expected_value = std::expected<napi_value, napi_value>;

class handle_scope : util::non_moveable {
	public:
		explicit handle_scope(napi_env env) :
				env_{env},
				handle_scope_{napi_invoke_checked(napi_open_handle_scope, env)} {}

		~handle_scope() {
			if (napi_close_handle_scope(env_, handle_scope_) != napi_ok) {
				std::terminate();
			}
		}

	private:
		napi_env env_;
		napi_handle_scope handle_scope_;
};

auto make_promise(environment& ienv, auto accept) {
	auto* env = ienv.nenv();

	// nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	auto* promise = napi_invoke_checked(napi_create_promise, env, &deferred);

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
					auto scope = handle_scope{env};
					auto& ienv = environment::get(env);
					ienv.scheduler().decrement_ref();
					if (auto result = accept(ienv, std::forward<decltype(args)>(args)...)) {
						napi_check_result_of(napi_resolve_deferred, env, deferred, result.value());
					} else {
						napi_check_result_of(napi_reject_deferred, env, deferred, result.error());
					}
				}
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

template <class Function>
class node_function;

template <class Result, class... Args>
class node_function<Result (*)(environment&, Args...)> : util::non_copyable {
	public:
		using function_type = Result (*)(environment&, Args...);

		node_function() = delete;
		explicit node_function(environment& env, function_type fn) :
				env{&env},
				fn{std::move(fn)} {}

		auto operator()(const Napi::CallbackInfo& info) -> napi_value {
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(*env),
					value::transfer<std::tuple<Args...>>(static_cast<napi_callback_info>(info), std::tuple{env->nenv(), env->isolate()}, std::tuple{})
				)
			);
		}

	private:
		environment* env;
		function_type fn;
};

export auto make_node_function(environment& env, auto fn) {
	return Napi::Function::New(env.nenv(), node_function<decltype(fn)>{env, fn});
}

} // namespace ivm
