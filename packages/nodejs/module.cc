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
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, js_module /*module_*/, std::vector<ivm::module_request> requests) -> expected_value {
			return value::transfer_strict<Napi::Value>(std::move(requests), std::tuple{}, std::tuple{env.napi_env()});
		}
	);
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
			return dispatch(std::move(module), requests);
		}
	);
	return promise;
}

auto make_compile_module(environment& env) -> Napi::Function {
	return make_node_function(env, compile_module);
}

} // namespace ivm
