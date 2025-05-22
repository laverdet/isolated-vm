module;
#include <tuple>
#include <type_traits>
#include <variant>
export module isolated_v8.function;
import isolated_js;
import isolated_v8.agent;
import isolated_v8.collected_handle;
import isolated_v8.realm;
import isolated_v8.remote;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

export class function_template {
	private:
		explicit function_template(agent::lock& agent, v8::Local<v8::FunctionTemplate> function);

	public:
		auto make_function(v8::Local<v8::Context> context) -> v8::Local<v8::Function>;

		static auto make(agent::lock& agent, auto&& function) -> function_template;

	private:
		shared_remote<v8::FunctionTemplate> function_;
};

// ---

// Helper specializing against a function signature to transfer out the arguments, and transfer in
// the result.
template <class Signature>
struct invoke_callback;

template <class Result, class... Args>
struct invoke_callback<Result(realm::scope&, Args...)> {
		constexpr static auto length = sizeof...(Args);

		auto operator()(const v8::FunctionCallbackInfo<v8::Value>& info, auto&& invocable) {
			auto& agent = agent::lock::get_current();
			realm::witness_scope realm{agent, agent->isolate()->GetCurrentContext()};
			auto run = [ & ]() -> decltype(auto) {
				return std::apply(
					std::forward<decltype(invocable)>(invocable),
					std::tuple_cat(
						std::forward_as_tuple(realm),
						js::transfer_out<std::tuple<Args...>>(info, realm)
					)
				);
			};
			auto run_with_result = [ & ]() -> decltype(auto) {
				if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
					run();
					return std::monostate{};
				} else {
					return run();
				}
			};
			js::transfer_in_strict<v8::ReturnValue<v8::Value>>(run_with_result(), realm, info.GetReturnValue());
		}
};

// Bound function factory
auto function_template::make(agent::lock& agent, auto&& function) -> function_template {
	using function_type = std::decay_t<decltype(function.invocable)>;
	using signature_type = util::function_signature_t<function_type>;

	if constexpr (std::is_empty_v<function_type>) {
		static_assert(false, "Not implemented");
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_destructible_v<function_type>) {
		static_assert(false, "Not implemented");
	} else {
		auto external = make_collected_external<function_type>(agent, agent->autorelease_pool(), std::forward<decltype(function)>(function).invocable);
		v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
			auto& invocable = unwrap_collected_external<function_type>(info.Data().As<v8::External>());
			invoke_callback<signature_type> invoke{};
			invoke(info, invocable);
		};
		auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, external, {}, invoke_callback<signature_type>::length, v8::ConstructorBehavior::kThrow);
		return function_template{agent, fn_template};
	}
}

} // namespace isolated_v8

namespace js {
using isolated_v8::function_template;

template <>
struct visit<void, function_template> : visit<void, v8::Local<v8::Value>> {
	public:
		using visit<void, v8::Local<v8::Value>>::visit;

		auto operator()(function_template value, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, value.make_function(witness().context()));
		}
};

} // namespace js
