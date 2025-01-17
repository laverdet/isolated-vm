module;
#include <array>
#include <span>
#include <stdexcept>
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
		static auto make(agent::lock& agent, js::bound_function<Invocable, Result(Args...)> function) -> function_template;

	private:
		shared_remote<v8::FunctionTemplate> function_;
};

// ---

template <class Invocable, class Result, class... Args>
auto function_template::make(agent::lock& agent, js::bound_function<Invocable, Result(Args...)> function) -> function_template {
	using function_type = decltype(function);
	auto external = make_collected_external<function_type>(agent, agent->autorelease_pool(), std::move(function));
	v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
		js::iv8::isolate_implicit_witness_lock isolate_lock{info.GetIsolate()};
		js::iv8::context_implicit_witness_lock context_lock{isolate_lock, isolate_lock.isolate()->GetCurrentContext()};
		std::array<v8::Local<v8::Value>, 8> arguments;
		auto count = static_cast<size_t>(info.Length());
		if (count > arguments.size()) {
			throw std::runtime_error{"Too many arguments"};
		}
		auto& functor = unwrap_collected_external<function_type>(info.Data().As<v8::External>());
		auto run = [ & ]() -> decltype(auto) {
			return std::apply(
				functor,
				js::transfer_out<std::tuple<Args...>>(std::span{arguments}.subspan(0, count), context_lock)
			);
		};
		if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
			info.GetReturnValue().SetUndefined();
			run();
		} else {
			js::transfer_in_strict<v8::ReturnValue<v8::Value>>(run(), context_lock, info.GetReturnValue());
		}
	};
	auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, external, {}, sizeof...(Args), v8::ConstructorBehavior::kThrow);
	return function_template{agent, fn_template};
}

} // namespace isolated_v8
