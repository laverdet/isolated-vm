module;
#include <concepts>
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

template <class Tuple>
struct dispatch_parameters;

template <class... Types>
struct dispatch_parameters<std::tuple<environment&, Types...>>
		: std::type_identity<std::tuple<Types...>> {
		static_assert(std::conjunction_v<std::is_move_constructible<Types>...>);
};

auto make_promise(environment& ienv, auto accept) {
	using tuple_type = dispatch_parameters<util::functor_parameters_t<decltype(accept)>>::type;

	// nodejs promise & future
	auto env = ienv.nenv();
	Napi::Promise::Deferred deferred{env};
	auto promise = deferred.Promise();

	// nodejs promise fulfillment
	auto settle =
		[ accept = std::move(accept),
			deferred ](
			Napi::Env env,
			tuple_type* payload_raw_ptr
		) mutable {
			if (env) {
				auto& ienv = environment::get(env);
				auto payload_ptr = ienv.collection().accept_ptr<tuple_type>(payload_raw_ptr);
				auto arguments = std::tuple_cat(std::forward_as_tuple(ienv), std::move(*payload_ptr));
				if (auto result = std::apply(accept, std::move(arguments))) {
					deferred.Resolve(result.value());
				} else {
					deferred.Reject(result.error());
				}
			}
		};

	// napi adapter to own complex types through the C boundary
	auto holder = ienv.collection().make_ptr<decltype(settle)>(std::move(settle));
	auto trampoline = [](Napi::Env env, Napi::Function /*unused*/, decltype(settle)* settle, tuple_type* payload_raw_ptr) {
		(*settle)(env, payload_raw_ptr);
	};
	auto finalizer = [](Napi::Env /*env*/, util::collection_group* collection, decltype(settle)* settle) {
		collection->collect(settle);
	};

	// Thread-safe -> nodejs callback
	auto& collection = ienv.collection();
	using Dispatcher = Napi::TypedThreadSafeFunction<decltype(settle), tuple_type, trampoline>;
	Dispatcher dispatcher = Dispatcher::New(env, "isolated-vm", 0, 1, holder.release(), finalizer, &collection);

	// Function with abandonment detection
	auto invoked = [ &collection ](Dispatcher dispatcher, tuple_type payload) {
		auto payload_holder = collection.make_ptr<tuple_type>(std::move(payload));
		dispatcher.BlockingCall(payload_holder.release());
		dispatcher.Release();
	};
	auto apply_invoked = [ invoked = std::move(invoked) ](Dispatcher dispatcher, auto&&... args) {
		return invoked(dispatcher, std::tuple{std::forward<decltype(args)>(args)...});
	};
	auto abandoned = [](Dispatcher dispatcher) {
		dispatcher.BlockingCall(nullptr);
		dispatcher.Release();
	};
	auto dispatch = util::call_once_or_else(std::move(apply_invoked), std::move(abandoned), std::move(dispatcher));

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
