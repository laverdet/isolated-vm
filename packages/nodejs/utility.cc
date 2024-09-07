module;
#include <expected>
#include <memory>
#include <tuple>
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
auto make_promise(Napi::Env env, collection_group& collection, auto accept)
	requires std::is_move_constructible_v<Type> &&
	std::is_invocable_r_v<expected_value, decltype(accept), Napi::Env, Type>
{
	// nodejs promise & future
	Napi::Promise::Deferred deferred{env};
	auto promise = deferred.Promise();

	// nodejs promise fulfillment
	auto settle =
		[ accept = std::move(accept),
			deferred,
			&collection ](
			Napi::Env env,
			Type* payload_raw_ptr
		) mutable {
			auto payload_ptr = collection.accept_ptr<Type>(payload_raw_ptr);
			if (env) {
				if (auto result = accept(env, std::move(std::move(*payload_ptr)))) {
					deferred.Resolve(result.value());
				} else {
					deferred.Reject(result.error());
				}
			}
		};

	// napi adapter to own complex types through the C boundary
	auto holder = collection.make_ptr<decltype(settle)>(std::move(settle));
	auto trampoline = [](Napi::Env env, Napi::Function /*unused*/, decltype(settle)* settle, Type* payload_raw_ptr) {
		(*settle)(env, payload_raw_ptr);
	};
	auto finalizer = [](Napi::Env /*env*/, collection_group* collection, decltype(settle)* settle) {
		collection->collect(settle);
	};

	// Thread-safe -> nodejs callback
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
	auto dispatch = call_once_or_else(std::move(invoked), std::move(abandoned), std::move(dispatcher));

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

template <class Function>
class node_function;

template <class Result, class... Args>
class node_function<Result (*)(Napi::Env, ivm::environment&, Args...)> : non_copyable {
	public:
		using function_type = Result (*)(Napi::Env, ivm::environment&, Args...);

		node_function() = delete;
		explicit node_function(ivm::environment& ienv, function_type fn) :
				ienv{&ienv},
				fn{std::move(fn)} {}

		auto operator()(const Napi::CallbackInfo& info) -> Napi::Value {
			ivm::napi::napi_callback_info_memo callback_info{info, *ienv};
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(info.Env(), *ienv),
					ivm::value::transfer<std::tuple<Args...>>(callback_info)
				)
			);
		}

	private:
		ivm::environment* ienv;
		function_type fn;
};

export auto make_node_function(Napi::Env env, ivm::environment& ienv, auto fn) {
	return Napi::Function::New(env, node_function<decltype(fn)>{ienv, fn});
}

} // namespace ivm
