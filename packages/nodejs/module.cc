module;
#include <concepts>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module ivm.node;
import :environment;
import :external;
import :script;
import :utility;
import :visit;
import ivm.isolated_v8;
import ivm.iv8;
import ivm.value;
import napi;

namespace ivm {

auto compile_module(
	environment& env,
	iv8::external_reference<agent>& agent,
	iv8::external_reference<realm>& realm_,
	value::string_t source_text
) -> Napi::Value {
	using result_type = std::tuple<ivm::js_module, std::vector<ivm::module_request>>;
	auto [ dispatch, promise ] = make_promise<result_type>(env, [](environment& env, auto result) -> expected_value {
		return value::transfer_strict<Napi::Value>(std::get<1>(result), std::tuple{}, std::tuple{env.napi_env()});
	});
	auto& realm = *realm_;
	agent->schedule(
		[ &realm, /* TODO: NO!*/
			source_text = std::move(source_text),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			ivm::source_origin source_origin{};
			ivm::realm::managed_scope realm_scope{agent, realm};
			auto module = ivm::js_module::compile(realm_scope, std::move(source_text), source_origin);
			auto requests = module.requests(realm_scope);
			return dispatch(std::tuple{std::move(module), std::move(requests)});
		}
	);
	return promise;
}

auto make_compile_module(environment& env) -> Napi::Function {
	return make_node_function(env, compile_module);
}

} // namespace ivm
