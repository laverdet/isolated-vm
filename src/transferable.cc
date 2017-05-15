#include "transferable.h"
#include "class_handle.h"
#include "transferable_handle.h"
#include "external_copy.h"

using namespace std;
using namespace v8;

namespace ivm {

unique_ptr<Transferable> Transferable::TransferOut(const Local<Value>& value) {
	if (value->IsObject()) {
		Local<Object> object = value.As<Object>();
		if (object->InternalFieldCount() > 0 && ClassHandle::GetFunctionTemplate<TransferableHandle>()->HasInstance(object)) {
			TransferableHandle* handle = ClassHandle::Unwrap<TransferableHandle>(object);
			return handle->TransferOut();
		}
	}
	unique_ptr<Transferable> copy = ExternalCopy::CopyIfPrimitive(value);
	if (copy.get() == nullptr) {
		Isolate::GetCurrent()->ThrowException(Exception::TypeError(v8_string("A non-transferable value was passed")));
	}
	return copy;
}

}
