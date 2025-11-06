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
		clock_time_{js::iv8::make_unique_remote(agent, js::iv8::function_template::make(agent, js::free_function{clock_time}))},
		performance_time_{js::iv8::make_unique_remote(agent, js::iv8::function_template::make(agent, js::free_function{performance_time}))} {
}

auto runtime_interface::instantiate(const isolated_v8::realm::scope& realm) -> isolated_v8::js_module {
	auto make_interface = [ & ]() {
		return std::vector{
			std::pair{std::string{"clockTime"}, clock_time_.deref(realm)},
			std::pair{std::string{"performanceTime"}, performance_time_.deref(realm)},
		};
	};
	auto options = js::string_t{"isolated-vm://runtime"};
	auto interface = isolated_v8::js_module::create_synthetic(realm, make_interface(), std::move(options));
	auto runtime = isolated_v8::js_module::compile(realm, runtime_dist_interface_js, isolated_v8::source_origin{});
	runtime.link(realm, [ & ](auto&&...) -> isolated_v8::js_module& {
		return interface;
	});
	runtime.evaluate(realm);
	return runtime;
}

} // namespace backend_napi_v8
