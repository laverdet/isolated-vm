module;
#include "runtime_dist_interface_js.h"
module backend_napi_v8;
import :lock;
import :runtime;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

auto clock_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::steady_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

auto performance_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::steady_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

auto transfer(
	const js::iv8::isolated::realm_scope& lock,
	js::forward<v8::Local<v8::Value>> subject,
	std::optional<js::iv8::value_of<js::list_tag>> transfer
) -> js::forward<v8::Local<v8::Value>> {
	return js::forward{transfer_record::make(lock, *subject, transfer)};
}

// transfer_record
transfer_record::transfer_record(
	v8::Isolate* isolate,
	v8::Local<v8::Value> subject,
	std::optional<js::iv8::value_of<js::list_tag>> transfer
) :
		subject_{isolate, subject},
		transfer_{isolate, transfer ? v8::Local<v8::Array>{*transfer} : v8::Local<v8::Array>{}} {}

auto transfer_record::make(
	const js::iv8::isolated::realm_scope& lock,
	v8::Local<v8::Value> subject,
	std::optional<js::iv8::value_of<js::list_tag>> transfer
) -> v8::Local<v8::Value> {
	return external_type::make_collected(lock, lock.isolate(), subject, transfer);
}

auto transfer_record::match(v8::Local<v8::Value> value) -> transfer_record* {
	if (value->IsExternal()) {
		return value.As<external_type>()->try_cast();
	} else {
		return nullptr;
	}
}

auto transfer_record::subject(js::iv8::isolate_lock_witness lock) const -> v8::Local<v8::Value> {
	return subject_.Get(lock.isolate());
}

auto transfer_record::transfer(js::iv8::context_lock_witness lock) const -> std::optional<js::iv8::value_of<js::list_tag>> {
	if (transfer_.IsEmpty()) {
		return std::nullopt;
	} else {
		return js::iv8::value_of{lock, transfer_.Get(lock.isolate())};
	}
}

// runtime_interface
runtime_interface::runtime_interface(const js::iv8::isolated::agent_lock& lock) :
		clock_time_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{clock_time}, lock))},
		performance_time_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{performance_time}, lock))},
		transfer_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{transfer}, lock))} {
}

auto runtime_interface::instantiate(js::iv8::context_lock_witness lock) -> v8::Local<js::iv8::module_record> {
	auto make_interface = [ & ] -> auto {
		return std::tuple{
			std::pair{util::cw<"clockTime">, clock_time_->deref(util::slice(lock))},
			std::pair{util::cw<"performanceTime">, performance_time_->deref(util::slice(lock))},
			std::pair{util::cw<"transfer">, transfer_->deref(util::slice(lock))},
		};
	};
	auto origin = std::u16string{u"isolated-vm://runtime"};
	auto interface = js::iv8::module_record::create_synthetic(lock, std::move(origin), make_interface());
	auto runtime = js::iv8::unmaybe(js::iv8::module_record::compile(lock, util::make_consteval_string_view(runtime_dist_interface_js), js::iv8::source_origin{}));
	auto link_record = js::iv8::module_link_record{
		.modules = {runtime, interface},
		.payload = {1, 1, 0},
	};
	runtime->link(lock, std::move(link_record));
	std::ignore = runtime->evaluate(lock).value();
	return runtime;
}

} // namespace backend_napi_v8
