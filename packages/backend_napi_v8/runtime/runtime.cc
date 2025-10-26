module;
#include "runtime/dist/interface.js.h"
#include <chrono>
module backend_napi_v8;
import :runtime;
import isolated_js;
import isolated_v8;
import ivm.utility;

namespace backend_napi_v8 {

auto clock_time(const isolated_v8::realm::scope& /*realm*/) -> double {
	auto now = std::chrono::utc_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

auto performance_time(const isolated_v8::realm::scope& /*realm*/) -> double {
	auto now = std::chrono::steady_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

runtime_interface::runtime_interface(const isolated_v8::agent_lock& agent) :
		clock_time_{isolated_v8::function_template::make(agent, js::free_function{clock_time})},
		performance_time_{isolated_v8::function_template::make(agent, js::free_function{performance_time})} {
}

auto runtime_interface::instantiate(const isolated_v8::realm::scope& realm) -> isolated_v8::js_module {
	auto make_interface = [ & ]() {
		return std::tuple{
			std::pair{util::cw<"clockTime">, clock_time_},
			std::pair{util::cw<"performanceTime">, performance_time_},
		};
	};
	auto options = isolated_v8::source_required_name{.name = "isolated-vm://runtime"};
	auto interface = isolated_v8::js_module::create_synthetic(realm.agent(), make_interface(), std::move(options));
	auto runtime = isolated_v8::js_module::compile(realm.agent(), runtime_dist_interface_js, isolated_v8::source_origin{});
	runtime.link(realm, [ & ](auto&&...) -> isolated_v8::js_module& {
		return interface;
	});
	runtime.evaluate(realm);
	return runtime;
}

} // namespace backend_napi_v8
