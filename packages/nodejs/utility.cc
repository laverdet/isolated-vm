module;
#include <cstring>
#include <expected>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.node:utility;
import :environment;
import :visit;
import ivm.utility;
import ivm.value;
import napi;

namespace ivm {

export using expected_value = std::expected<Napi::Value, Napi::Value>;

export template <std::move_constructible Type>
auto make_promise(environment& ienv, auto accept)
	requires std::is_move_constructible_v<Type> &&
	std::is_invocable_r_v<expected_value, decltype(accept), environment&, Type>
{
	// nodejs promise & future
	auto env = ienv.napi_env();
	Napi::Promise::Deferred deferred{env};
	auto promise = deferred.Promise();

	// nodejs promise fulfillment
	auto settle =
		[ accept = std::move(accept),
			deferred ](
			Napi::Env env,
			Type* payload_raw_ptr
		) mutable {
			if (env) {
				auto& ienv = environment::get(env);
				auto payload_ptr = ienv.collection().accept_ptr<Type>(payload_raw_ptr);
				if (auto result = accept(ienv, std::move(std::move(*payload_ptr)))) {
					deferred.Resolve(result.value());
				} else {
					deferred.Reject(result.error());
				}
			}
		};

	// napi adapter to own complex types through the C boundary
	auto holder = ienv.collection().make_ptr<decltype(settle)>(std::move(settle));
	auto trampoline = [](Napi::Env env, Napi::Function /*unused*/, decltype(settle)* settle, Type* payload_raw_ptr) {
		(*settle)(env, payload_raw_ptr);
	};
	auto finalizer = [](Napi::Env /*env*/, util::collection_group* collection, decltype(settle)* settle) {
		collection->collect(settle);
	};

	// Thread-safe -> nodejs callback
	auto& collection = ienv.collection();
	using Dispatcher = Napi::TypedThreadSafeFunction<decltype(settle), Type, trampoline>;
	Dispatcher dispatcher = Dispatcher::New(env, "isolated-vm", 0, 1, holder.release(), finalizer, &collection);

	// Function with abandonment detection
	auto invoked = [ &collection ](Dispatcher dispatcher, Type payload) {
		auto payload_holder = collection.make_ptr<Type>(std::move(payload));
		dispatcher.BlockingCall(payload_holder.release());
		dispatcher.Release();
	};
	auto abandoned = [](Dispatcher dispatcher) {
		dispatcher.BlockingCall(nullptr);
		dispatcher.Release();
	};
	auto dispatch = util::call_once_or_else(std::move(invoked), std::move(abandoned), std::move(dispatcher));

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

		auto operator()(const Napi::CallbackInfo& info) -> Napi::Value {
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(*env),
					value::transfer<std::tuple<Args...>>(info, std::tuple{env->napi_env(), env->isolate()}, std::tuple{})
				)
			);
		}

	private:
		environment* env;
		function_type fn;
};

export auto make_node_function(environment& env, auto fn) {
	return Napi::Function::New(env.napi_env(), node_function<decltype(fn)>{env, fn});
}

} // namespace ivm
