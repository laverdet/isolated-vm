module;
#include <bit>
#include <tuple>
#include <type_traits>
#include <variant>
export module isolated_v8:function;
import isolated_js;
import :agent_handle;
import :realm;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

export class function_template {
	private:
		explicit function_template(const agent_lock& agent, v8::Local<v8::FunctionTemplate> function);

	public:
		auto make_function(const js::iv8::context_lock_witness& lock) -> v8::Local<v8::Function>;

		static auto make(const agent_lock& agent, auto function) -> function_template;

	private:
		js::iv8::shared_remote<v8::FunctionTemplate> function_;
};

// ---

// Helper specializing against a function signature to transfer out the arguments, and transfer in
// the result.
template <class Signature>
struct invoke_callback;

template <class Result, class... Args>
struct invoke_callback<auto(const realm::scope&, Args...)->Result> {
		constexpr static auto length = sizeof...(Args);

		auto operator()(const v8::FunctionCallbackInfo<v8::Value>& info, auto& invocable) -> void {
			auto& host = *agent_host::get_current();
			auto agent = agent_lock{js::iv8::isolate_execution_lock::make_witness(host.isolate()), host};
			auto context = host.isolate()->GetCurrentContext();
			auto realm = realm::scope{js::iv8::context_lock_witness::make_witness(agent, context), *agent};
			auto run = util::regular_return{[ & ]() -> decltype(auto) {
				return std::apply(
					invocable,
					std::tuple_cat(
						std::forward_as_tuple(realm),
						js::transfer_out<std::tuple<Args...>>(info, realm)
					)
				);
			}};
			js::transfer_in_strict<v8::ReturnValue<v8::Value>>(run().value_or(std::monostate{}), realm, info.GetReturnValue());
		}
};

// Bound function factory
auto function_template::make(const agent_lock& agent, auto function) -> function_template {
	using function_type = std::remove_cvref_t<decltype(function.callback)>;
	using signature_type = util::function_signature_t<function_type>;

	if constexpr (std::is_empty_v<function_type>) {
		static_assert(std::is_trivially_constructible_v<function_type>);
		v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
			auto callback = function_type{};
			invoke_callback<signature_type> invoke{};
			invoke(info, callback);
		};
		auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, {}, {}, invoke_callback<signature_type>::length, v8::ConstructorBehavior::kThrow);
		return function_template{agent, fn_template};
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_destructible_v<function_type>) {
		auto external = v8::External::New(agent.isolate(), std::bit_cast<void*>(function.callback));
		v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
			auto callback = std::bit_cast<function_type>(info.Data().As<v8::External>()->Value());
			invoke_callback<signature_type> invoke{};
			invoke(info, callback);
		};
		auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, external, {}, invoke_callback<signature_type>::length, v8::ConstructorBehavior::kThrow);
		return function_template{agent, fn_template};
	} else {
		auto external = js::iv8::make_collected_external<function_type>(agent, std::move(function.callback));
		v8::FunctionCallback callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
			auto& callback = js::iv8::unwrap_collected_external<function_type>(info.Data().As<v8::External>());
			invoke_callback<signature_type> invoke{};
			invoke(info, callback);
		};
		auto fn_template = v8::FunctionTemplate::New(agent.isolate(), callback, external, {}, invoke_callback<signature_type>::length, v8::ConstructorBehavior::kThrow);
		return function_template{agent, fn_template};
	}
}

} // namespace isolated_v8

namespace js {
using isolated_v8::function_template;

template <class Meta>
struct visit<Meta, function_template> : visit<Meta, v8::Local<v8::Value>> {
	public:
		using visit_type = visit<Meta, v8::Local<v8::Value>>;
		using visit_type::lock_witness;
		using visit_type::visit_type;

		template <class Accept>
		auto operator()(function_template value, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(function_tag{}, *this, value.make_function(lock_witness()));
		}
};

} // namespace js
