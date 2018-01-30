#include "transferable.h"
#include "isolate/class_handle.h"
#include "isolate/util.h"
#include "transferable_handle.h"
#include "external_copy.h"

using namespace std;
using namespace v8;

namespace ivm {

unique_ptr<Transferable> Transferable::TransferOut(const Local<Value>& value) {
	if (value->IsObject()) {
		Local<Object> object = value.As<Object>();
		if (object->InternalFieldCount() > 0 && ClassHandle::GetFunctionTemplate<TransferableHandle>()->HasInstance(object)) {
			ClassHandle* handle = ClassHandle::Unwrap(object);
			return dynamic_cast<TransferableHandle*>(handle)->TransferOut();
		}
	}
	unique_ptr<Transferable> copy = ExternalCopy::CopyIfPrimitive(value);
	if (copy.get() == nullptr) {
		throw js_type_error("A non-transferable value was passed");
	}
	return copy;
}

}
