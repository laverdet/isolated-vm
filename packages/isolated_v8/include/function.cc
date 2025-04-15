module;
#include <array>
#include <span>
#include <stdexcept>
#include <tuple>
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

		template <class Invocable, class Result, class... Args>
		static auto make(agent::lock& agent, js::bound_function<Invocable, Result(realm::scope&, Args...)> function) -> function_template;
		template <auto Function, class Result, class... Args>
		static auto make(agent::lock& agent, js::free_function<Function, Result(realm::scope&, Args...)> function) -> function_template;

	private:
		shared_remote<v8::FunctionTemplate> function_;
};

// ---

template <class... Args>
auto invoke_callback(const v8::FunctionCallbackInfo<v8::Value>& info, auto&& invocable) -> void {
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
	if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
		info.GetReturnValue().SetUndefined();
		run();
	} else {
		js::transfer_in_strict<v8::ReturnValue<v8::Value>>(run(), realm, info.GetReturnValue());
	}
}

// Bound function factory
template <class Invocable, class Result, class... Args>
auto function_template::make(agent::lock& agent, js::bound_function<Invocable, Result(realm::scope&, Args...)> invocable) -> function_template {
	using invocable_type = decltype(invocable);
	auto external = make_collected_external<invocable_type>(agent, agent->autorelease_pool(), std::move(invocable));
	v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
		auto& invocable = unwrap_collected_external<invocable_type>(info.Data().As<v8::External>());
		invoke_callback<Args...>(info, invocable);
	};
	auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, external, {}, sizeof...(Args), v8::ConstructorBehavior::kThrow);
	return function_template{agent, fn_template};
}

// Free function factory
template <auto Function, class Result, class... Args>
auto function_template::make(agent::lock& agent, js::free_function<Function, Result(realm::scope&, Args...)> /*function*/) -> function_template {
	v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
		invoke_callback<Args...>(info, Function);
	};
	auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, {}, {}, sizeof...(Args), v8::ConstructorBehavior::kThrow);
	return function_template{agent, fn_template};
}

} // namespace isolated_v8

namespace js {
using isolated_v8::function_template;

template <>
struct visit<void, function_template> : visit<void, v8::Local<v8::Value>> {
	public:
		using visit<void, v8::Local<v8::Value>>::visit;

		auto operator()(function_template value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, value.make_function(witness().context()));
		}
};

} // namespace js
