module;
#include "runtime/dist/interface.js.h"
#include <chrono>
module backend_napi_v8;
import :lock;
import :runtime;
import auto_js;
import util;
import v8_js;

namespace backend_napi_v8 {

auto clock_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::utc_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

auto performance_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::steady_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

runtime_interface::runtime_interface(const js::iv8::isolated::agent_lock& lock) :
		clock_time_{make_unique_remote(lock, js::iv8::function_template::make(lock, js::free_function{clock_time}))},
		performance_time_{make_unique_remote(lock, js::iv8::function_template::make(lock, js::free_function{performance_time}))} {
}

auto runtime_interface::instantiate(js::iv8::context_lock_witness lock) -> v8::Local<v8::Module> {
	auto make_interface = [ & ]() -> auto {
		return std::vector{
			std::pair{std::string{"clockTime"}, clock_time_->deref(util::slice{lock})},
			std::pair{std::string{"performanceTime"}, performance_time_->deref(util::slice{lock})},
		};
	};
	auto origin = std::u16string{u"isolated-vm://runtime"};
	auto interface = js::iv8::module_record::create_synthetic(lock, make_interface(), std::move(origin));
	auto runtime = js::iv8::module_record::compile(lock, std::string_view{runtime_dist_interface_js}, js::iv8::source_origin{}).value();
	auto link_record = js::iv8::module_link_record{
		.modules = {runtime, interface},
		.payload = {1, 1, 0},
	};
	js::iv8::module_record::link(lock, runtime, std::move(link_record));
	js::iv8::module_record::evaluate(lock, runtime);
	return runtime;
}

} // namespace backend_napi_v8
