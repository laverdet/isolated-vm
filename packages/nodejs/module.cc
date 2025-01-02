module;
#include <concepts>
#include <future>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module ivm.node;
import :environment;
import :external;
import :utility;
import ivm.isolated_v8;
import ivm.iv8;
import ivm.js;
import ivm.napi;
import napi;

namespace ivm {

struct compile_module_options {
		std::optional<source_origin> origin;
};

auto compile_module(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::iv8::external_reference<realm>& realm_,
	js::string_t source_text,
	std::optional<compile_module_options> options_optional
) -> napi_value {
	auto options = std::move(options_optional).value_or(compile_module_options{});
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, js_module module_, std::vector<ivm::module_request> requests) -> expected_value {
			auto handle = make_collected_external<ivm::js_module>(env, std::move(module_));
			return js::transfer_strict<napi_value>(
				std::tuple{
					js::transfer_direct{std::move(handle)},
					std::move(requests),
				},
				std::tuple{},
				std::tuple{env.nenv()}
			);
		}
	);
	auto& realm = *realm_;
	agent->schedule(
		[ &realm, /* TODO: NO!*/
			options = std::move(options),
			source_text = std::move(source_text),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			ivm::realm::managed_scope realm_scope{agent, realm};
			auto origin = std::move(options.origin).value_or(source_origin{});
			auto module = ivm::js_module::compile(realm_scope, std::move(source_text), origin);
			auto requests = module.requests(realm_scope);
			dispatch(std::move(module), requests);
		}
	);
	return promise;
}

auto make_compile_module(environment& env) -> napi_value {
	return make_node_function<compile_module>(env);
}

} // namespace ivm

namespace ivm::js {

template <>
struct object_properties<compile_module_options> {
		constexpr static auto properties = std::tuple{
			member<"origin", &compile_module_options::origin, false>{},
		};
};

} // namespace ivm::js
