module isolated_vm;
import :support.cast;
import :support.lock;
import :value.function;
import v8_js;

namespace isolated_vm {
using namespace js;

// value_for_function
auto value_for_function::invoke(const runtime_lock& lock, value_of<> that, std::span<value_of<>> argv) -> value_of<> {
	auto witness = lock.witness();
	auto* v8_argv = reinterpret_cast<v8::Local<v8::Value>*>(argv.data());
	auto result =
		cast_in(*this)->Call(witness.isolate(), witness.context(), cast_in(that), static_cast<int>(argv.size()), v8_argv);
	return cast_out(iv8::unmaybe(result));
}

} // namespace isolated_vm
