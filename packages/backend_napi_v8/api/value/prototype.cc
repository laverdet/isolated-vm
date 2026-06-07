module isolated_vm;
import :support.callback;
import :support.cast;
import :support.lock;
import :value;
import auto_js;

namespace isolated_vm {
using namespace js;

auto value_for_prototype::make(const basic_lock& lock, runtime_callback_data_allocated_type data, int length) -> value_of<function_prototype_tag> {
	auto [ v8_callback, v8_data ] = make_function_callback(lock.witness(), std::move(data));
	auto v8_function = v8::FunctionTemplate::New(lock.witness().isolate(), v8_callback, v8_data, {}, length, v8::ConstructorBehavior::kThrow);
	return cast_out(v8_function);
}

auto value_for_prototype::make(const basic_lock& lock, runtime_callback_data_span_type data, int length) -> value_of<function_prototype_tag> {
	auto [ v8_callback, v8_data ] = make_function_callback(lock.witness(), data);
	auto v8_function = v8::FunctionTemplate::New(lock.witness().isolate(), v8_callback, v8_data, {}, length, v8::ConstructorBehavior::kThrow);
	return cast_out(v8_function);
}

} // namespace isolated_vm
