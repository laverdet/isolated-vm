module backend_napi_v8;
import :compression;
import :encoding;
import :lock;
import :reference;
import :runtime;
import :zone;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

auto clock_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::system_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

auto performance_time(js::iv8::context_lock_witness /*lock*/) -> double {
	auto now = std::chrono::steady_clock::now();
	return duration_cast<js_clock::duration>(now.time_since_epoch()).count();
}

// Helper for `enqueue_microtask`
auto run_microtask(const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
	auto lock = js::iv8::context_lock_witness{info};
	auto* isolate = lock.isolate();
	auto& executor = js::iv8::isolated::agent_host::get_current(isolate).executor();
	auto* zone = zone_handle::try_cast(util::slice(lock), isolate->GetContinuationPreservedEmbedderData());
	auto timeout_scope = zone_handle::timeout_type::scope{executor, zone == nullptr ? nullptr : zone->timeout_timer()};
	auto callback = info.DataV2().As<v8::Function>();
	std::ignore = callback->Call(lock.context(), v8::Undefined(isolate), 0, nullptr);
}

// `enqueueMicrotask`
auto enqueue_microtask(const realm_scope& lock, js::forward<v8::Local<js::iv8::Function>> callback) -> void {
	auto context = lock.context();
	auto wrapper = js::iv8::unmaybe(v8::Function::New(context, run_microtask, *callback, 0, v8::ConstructorBehavior::kThrow));
	context->GetMicrotaskQueue()->EnqueueMicrotask(lock.isolate(), wrapper);
}

// `getPromiseInternal`
auto get_promise_internal(js::iv8::context_lock_witness lock, js::forward<v8::Local<v8::Promise>> promise) -> js::forward<v8::Local<v8::Value>> {
	auto settled = [ & ](bool fulfilled) -> auto {
		return js::forward{js::transfer_in<v8::Local<v8::Value>>(std::tuple{fulfilled, js::forward{(*promise)->Result()}}, lock)};
	};
	switch ((*promise)->State()) {
		case v8::Promise::kPending: return js::forward{js::transfer_in<v8::Local<v8::Value>>(std::monostate{}, lock)};
		case v8::Promise::kFulfilled: return settled(true);
		case v8::Promise::kRejected: return settled(false);
	}
	std::unreachable();
}

auto transfer(
	const realm_scope& lock,
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
	const realm_scope& lock,
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

auto transfer_record::transfer(js::iv8::context_lock_witness lock) const -> js::iv8::value_of<js::list_tag> {
	return js::iv8::value_of{lock, transfer_.Get(lock.isolate())};
}

// runtime_interface
runtime_interface::runtime_interface(std::monostate /*nothing*/, const agent_lock& lock) :
		clock_time_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{clock_time}, lock))},
		enqueue_microtask_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{enqueue_microtask}, lock))},
		get_promise_internal_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{get_promise_internal}, lock))},
		performance_time_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{performance_time}, lock))},
		transfer_{make_unique_remote(lock, js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{transfer}, lock))} {
}

auto runtime_interface::instantiate(const realm_scope& lock) -> v8::Local<js::iv8::module_record> {
	auto make_interface = [ & ] -> auto {
		return std::tuple{
			std::pair{util::cw<"clockTime">, clock_time_->deref(util::slice(lock))},
			std::pair{util::cw<"enqueueMicrotask">, enqueue_microtask_->deref(util::slice(lock))},
			std::pair{util::cw<"getPromiseInternal">, get_promise_internal_->deref(util::slice(lock))},
			std::pair{util::cw<"performanceTime">, performance_time_->deref(util::slice(lock))},
			std::pair{util::cw<"transfer">, transfer_->deref(util::slice(lock))},
			std::pair{util::cw<"Compressor">, v8::Local<v8::FunctionTemplate>{compressor_handle::class_template(lock)}},
			std::pair{util::cw<"Decompressor">, v8::Local<v8::FunctionTemplate>{decompressor_handle::class_template(lock)}},
			std::pair{util::cw<"Reference">, v8::Local<v8::FunctionTemplate>{reference_handle::class_template(lock)}},
			std::pair{util::cw<"StringReader">, v8::Local<v8::FunctionTemplate>{string_reader_handle::class_template(lock)}},
			std::pair{util::cw<"StringWriter">, v8::Local<v8::FunctionTemplate>{string_writer_handle::class_template(lock)}},
			std::pair{util::cw<"Zone">, v8::Local<v8::FunctionTemplate>{zone_handle::class_template(lock)}},
		};
	};
	auto origin = util::make_consteval_string_view("isolated-vm:runtime");
	return js::iv8::module_record::create_synthetic(lock, origin, make_interface());
}

} // namespace backend_napi_v8
