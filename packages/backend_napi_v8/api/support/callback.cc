module isolated_vm:support.callback;
import :support.callback_info;
import :support.callback_storage;
import :support.cast;
import :support.lock;
import std;
import util;
import v8_js;
import v8;

namespace isolated_vm {
using namespace js;

auto make_function_callback(iv8::isolate_lock_witness witness, runtime_callback_data_span_type data) {
	// Trivial data-only function type. `Data()` is a latin1 string containing the callback & function data.
	if (data.size() > max_callback_storage_size) {
		throw js::runtime_error{u"function data too large"};
	}
	auto& storage = *reinterpret_cast<runtime_callback_data_storage*>(data.data());
	storage.data = nullptr;
	auto v8_callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
		auto* isolate = info.GetIsolate();
		auto witness = iv8::context_lock_witness::from_isolate(iv8::isolate_lock_witness::make_witness(isolate));
		auto data_string = info.Data().As<v8::String>();
		auto length = data_string->Length();
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		std::array<std::byte, max_callback_storage_size> data;
		data_string->WriteOneByteV2(isolate, 0, length, reinterpret_cast<std::uint8_t*>(data.data()), v8::String::WriteFlags::kNone);
		const auto& storage = *reinterpret_cast<runtime_callback_data_storage*>(data.data());
		auto runtime_lock = runtime_lock_implementation{witness};
		info.GetReturnValue().Set(cast_in((*storage.callback)(runtime_lock, callback_info_implementation{info}, storage.data)));
	}};
	auto storage_sv = std::string_view{reinterpret_cast<const char*>(data.data()), data.size()};
	auto v8_data = js::transfer_in_strict<v8::Local<v8::String>>(storage_sv, witness);
	return std::tuple{v8_callback, v8_data};
}

auto make_function_callback(iv8::isolate_lock_witness witness, runtime_callback_data_allocated_type /*data*/) {
	throw std::logic_error{"not supported"};
	return make_function_callback(witness, std::span<std::byte>{});
}

} // namespace isolated_vm
