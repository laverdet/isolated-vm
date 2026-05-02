export module isolated_vm:support.lock;
import :support.lock_fwd;
import std;
import v8_js;

namespace isolated_vm {

basic_lock::basic_lock(void* data) : basic_data_{data} {}

auto basic_lock::witness() const {
	auto* isolate = std::bit_cast<v8::Isolate*>(basic_data_);
	return js::iv8::isolate_lock_witness::make_witness(isolate);
}

export class basic_lock_implementation : public basic_lock {
	public:
		explicit basic_lock_implementation(js::iv8::isolate_lock_witness lock) :
				basic_lock{lock.isolate()} {}
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
runtime_lock::runtime_lock(void* basic_data, void* runtime_data) :
		basic_lock{basic_data},
		runtime_data_{runtime_data} {}

auto runtime_lock::witness() const {
	auto* isolate = std::bit_cast<v8::Isolate*>(basic_data_);
	auto context = std::bit_cast<v8::Local<v8::Context>>(runtime_data_);
	auto witness = js::iv8::isolate_lock_witness::make_witness(isolate);
	return js::iv8::context_lock_witness::make_witness(witness, context);
}

export class runtime_lock_implementation : public runtime_lock {
	public:
		explicit runtime_lock_implementation(js::iv8::context_lock_witness lock) :
				runtime_lock{lock.isolate(), std::bit_cast<void*>(lock.context())} {}
};

} // namespace isolated_vm
