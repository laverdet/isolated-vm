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
		auto ptr = ClassHandle::Unwrap<TransferableHandle>(value.As<Object>());
		if (ptr != nullptr) {
			return ptr->TransferOut();
		}
	}
	unique_ptr<Transferable> copy = ExternalCopy::CopyIfPrimitive(value);
	if (!copy) {
		throw js_type_error("A non-transferable value was passed");
	}
	return copy;
}

} // namespace ivm
