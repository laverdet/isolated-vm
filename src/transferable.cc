#include "transferable.h"
#include "shareable_persistent.h"
#include "class_handle.h"

#include "external_copy.h"
#include "reference_handle.h"

using namespace std;
using namespace v8;

namespace ivm {

	/*
class TransferableExternalCopy : public Transferable {
	private:
		shared_ptr<ExternalCopy> value;

	public:
		TransferableExternalCopy(shared_ptr<ExternalCopy> value) : value(value) {}

		virtual Local<Value> TransferIn() {
			return value->CopyInto();
		}
};
*/

class TransferableReferenceHandle : public Transferable {
	private:
		shared_ptr<ShareablePersistent<Value>> reference;
		shared_ptr<ShareablePersistent<Context>> context;

	public:
		TransferableReferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference, shared_ptr<ShareablePersistent<Context>>& context) : reference(reference), context(context) {}

		virtual Local<Value> TransferIn() {
			return ClassHandle::NewInstance<ReferenceHandle>(reference, context);
		}
};

class TransferableDereferenceHandle : public Transferable {
	private:
		shared_ptr<ShareablePersistent<Value>> reference;

	public:
		TransferableDereferenceHandle(shared_ptr<ShareablePersistent<Value>>& reference) : reference(reference) {}

		virtual Local<Value> TransferIn() {
			Isolate* isolate = Isolate::GetCurrent();
			if (isolate == reference->GetIsolate()) {
				return reference->Deref();
			} else {
				isolate->ThrowException(Exception::Error(v8_string("Cannot dereference this from target isolate")));
				return Local<Value>();
			}
		}
};

unique_ptr<Transferable> Transferable::TransferOut(const Local<Value>& value) {
	if (value->IsObject()) {
		Local<Object> object = value.As<Object>();
		if (object->InternalFieldCount() > 0) {
			if (ClassHandle::GetFunctionTemplate<ReferenceHandle>()->HasInstance(object)) {
				// Reference handle
				ReferenceHandle& reference = *ClassHandle::Unwrap<ReferenceHandle>(object);
				return make_unique<TransferableReferenceHandle>(reference.reference, reference.context);

			} else if (ClassHandle::GetFunctionTemplate<DereferenceHandle>()->HasInstance(object)) {
				// Dereference handle
				DereferenceHandle& reference = *ClassHandle::Unwrap<DereferenceHandle>(object);
				return make_unique<TransferableDereferenceHandle>(reference.reference);
			}
		}
	}
	return ExternalCopy::CopyIfPrimitive(value);
}

}
