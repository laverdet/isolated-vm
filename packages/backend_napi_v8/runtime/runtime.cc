module;
#include "runtime/dist/interface.js.h"
#include <chrono>
module backend_napi_v8;
import :runtime;
import isolated_js;
import isolated_v8;
import ivm.utility;
namespace v8 = embedded_v8;

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

auto runtime_interface::instantiate(const isolated_v8::realm::scope& realm) -> v8::Local<v8::Module> {
	auto make_interface = [ & ]() -> auto {
		return std::vector{
			std::pair{std::string{"clockTime"}, clock_time_->deref(realm)},
			std::pair{std::string{"performanceTime"}, performance_time_->deref(realm)},
		};
	};
	auto origin = std::u16string{u"isolated-vm://runtime"};
	auto interface = js::iv8::module_record::create_synthetic(realm, make_interface(), std::move(origin));
	auto runtime = js::iv8::module_record::compile(realm, std::string_view{runtime_dist_interface_js}, js::iv8::source_origin{}).value();
	auto link_record = js::iv8::module_link_record{
		.modules = {runtime, interface},
		.payload = {1, 1, 0},
	};
	js::iv8::module_record::link(realm, runtime, std::move(link_record));
	js::iv8::module_record::evaluate(realm, runtime);
	return runtime;
}

} // namespace backend_napi_v8
