#include "class_handle.h"
namespace ivm {

ClassHandle* _ClassHandleUnwrap(v8::Local<v8::Object> handle) {
	return ClassHandle::UnwrapClassHandle(handle);
}

} // namespace ivm
