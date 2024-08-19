module;
#include <expected>
#include <memory>
export module ivm.node:make_promise;
import napi;
import ivm.utility;

namespace ivm {

export using expected_value = std::expected<Napi::Value, Napi::Value>;

export template <class Type>
auto make_promise(Napi::Env env, auto accept)
	requires std::is_move_constructible_v<Type> &&
	std::is_invocable_r_v<expected_value, decltype(accept), Napi::Env, std::unique_ptr<Type>>
{
	// nodejs promise & future
	Napi::Promise::Deferred deferred{env};
	auto promise = deferred.Promise();

	// nodejs promise fulfillment
	auto settle =
		[ accept = std::move(accept),
			deferred ](
			Napi::Env env,
			std::unique_ptr<Type> payload
		) mutable {
			if (env) {
				if (auto result = accept(env, std::move(payload))) {
					deferred.Resolve(result.value());
				} else {
					deferred.Reject(result.error());
				}
			}
		};

	// napi adapter to own complex types through the C boundary
	auto holder = std::make_unique<decltype(settle)>(std::move(settle));
	auto trampoline =
		[](
			Napi::Env env,
			Napi::Function /*unused*/,
			decltype(settle)* settle,
			Type* payload
		) {
			(*settle)(env, std::unique_ptr<Type>{payload});
		};

	// Thread-safe -> nodejs callback
	using Dispatcher = Napi::TypedThreadSafeFunction<decltype(settle), Type, trampoline>;
	Dispatcher dispatcher = Dispatcher::New(env, "isolated-vm", 0, 1, holder.release());

	// Function with abandonment detection
	auto invoked = [](Dispatcher dispatcher, Type payload) {
		auto holder = std::make_unique<Type>(std::move(payload));
		dispatcher.BlockingCall(holder.release());
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

} // namespace ivm
