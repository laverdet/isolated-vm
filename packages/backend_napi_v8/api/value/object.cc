module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;

namespace isolated_vm {
using namespace js;

// value_for_object
auto value_for_object::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsArray()) {
		return value_typeof::array;
	} else if (subject->IsExternal()) {
		return value_typeof::external;
	} else if (subject->IsDate()) {
		return value_typeof::date;
	} else if (subject->IsArrayBuffer()) {
		return value_typeof::array_buffer;
	} else if (subject->IsSharedArrayBuffer()) {
		return value_typeof::shared_array_buffer;
	} else if (subject->IsArrayBufferView()) {
		return value_of<array_buffer_view_tag>::from(*this).inspect();
	} else if (subject->IsPromise()) {
		return value_typeof::promise;
	} else if (subject->IsFunction()) {
		return value_typeof::function;
	} else {
		return value_typeof::dictionary;
	}
}

} // namespace isolated_vm
