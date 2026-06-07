module isolated_vm;
import :support.cast;
import :support.lock;
import :value.array;
import v8_js;

namespace isolated_vm {
using namespace js;

// value_for_array
auto value_for_array::set(const runtime_lock& lock, int key, value_of<> value) const -> void {
	iv8::unmaybe(cast_in(*this)->Set(lock.witness().context(), static_cast<std::uint32_t>(key), cast_in(value)));
}

auto value_for_array::make(const runtime_lock& lock, int capacity) -> value_of<list_tag> {
	return value_of<list_tag>::from(cast_out(v8::Array::New(lock.witness().isolate(), capacity)));
}

} // namespace isolated_vm
