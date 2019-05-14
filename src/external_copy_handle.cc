#include "external_copy_handle.h"
#include "external_copy.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * Transferable wrapper
 */
ExternalCopyHandle::ExternalCopyTransferable::ExternalCopyTransferable(std::shared_ptr<ExternalCopy> value) : value(std::move(value)) {}

Local<Value> ExternalCopyHandle::ExternalCopyTransferable::TransferIn() {
	return ClassHandle::NewInstance<ExternalCopyHandle>(value);
}

/**
 * ExternalCopyHandle implementation
 */
ExternalCopyHandle::ExternalCopyHandle(shared_ptr<ExternalCopy> value) : value(std::move(value)) {
	Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(this->value->OriginalSize());
}

ExternalCopyHandle::~ExternalCopyHandle() {
	if (value) {
		Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-static_cast<ptrdiff_t>(value->OriginalSize()));
	}
}

Local<FunctionTemplate> ExternalCopyHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"ExternalCopy", ParameterizeCtor<decltype(&New), &New>(),
		"totalExternalSize", ParameterizeStaticAccessor<decltype(&ExternalCopyHandle::TotalExternalSizeGetter), &ExternalCopyHandle::TotalExternalSizeGetter>(),
		"copy", Parameterize<decltype(&ExternalCopyHandle::Copy), &ExternalCopyHandle::Copy>(),
		"copyInto", Parameterize<decltype(&ExternalCopyHandle::CopyInto), &ExternalCopyHandle::CopyInto>(),
		"release", Parameterize<decltype(&ExternalCopyHandle::Release), &ExternalCopyHandle::Release>()
	));
}

unique_ptr<Transferable> ExternalCopyHandle::TransferOut() {
	return std::make_unique<ExternalCopyTransferable>(value);
}

unique_ptr<ExternalCopyHandle> ExternalCopyHandle::New(Local<Value> value, MaybeLocal<Object> maybe_options) {
	Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
	Local<Object> options;
	bool transfer_out = false;
	handle_vector_t transfer_list;
	if (maybe_options.ToLocal(&options)) {
		transfer_out = IsOptionSet(Isolate::GetCurrent()->GetCurrentContext(), options, "transferOut");
		Local<Value> transfer_list_handle = Unmaybe(options->Get(context, v8_string("transferList")));
		if (!transfer_list_handle->IsUndefined()) {
			if (!transfer_list_handle->IsArray()) {
				throw js_type_error("`transferList` must be an array");
			}
			size_t length = transfer_list_handle.As<Array>()->Length();
			transfer_list.reserve(length);
			for (size_t ii = 0; ii < length; ++ii) {
				transfer_list.emplace_back(Unmaybe(transfer_list_handle.As<Array>()->Get(context, ii)));
			}
		}
	}
	return std::make_unique<ExternalCopyHandle>(shared_ptr<ExternalCopy>(ExternalCopy::Copy(value, transfer_out, transfer_list)));
}

void ExternalCopyHandle::CheckDisposed() {
	if (!value) {
		throw js_generic_error("Copy has been released");
	}
}

/**
 * JS API functions
 */
Local<Value> ExternalCopyHandle::TotalExternalSizeGetter() {
	return Number::New(Isolate::GetCurrent(), ExternalCopy::TotalExternalSize());
}

Local<Value> ExternalCopyHandle::Copy(MaybeLocal<Object> maybe_options) {
	CheckDisposed();
	Local<Object> options;
	bool release = false;
	bool transfer_in = false;
	if (maybe_options.ToLocal(&options)) {
		Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
		release = IsOptionSet(context, options, "release");
		transfer_in = IsOptionSet(context, options, "transferIn");
	}
	Local<Value> ret = value->CopyIntoCheckHeap(transfer_in);
	if (release) {
		Release();
	}
	return ret;
}

Local<Value> ExternalCopyHandle::CopyInto(MaybeLocal<Object> maybe_options) {
	CheckDisposed();
	Local<Object> options;
	bool release = false;
	bool transfer_in = false;
	if (maybe_options.ToLocal(&options)) {
		Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
		release = IsOptionSet(context, options, "release");
		transfer_in = IsOptionSet(context, options, "transferIn");
	}
	Local<Value> ret = ClassHandle::NewInstance<ExternalCopyIntoHandle>(value, transfer_in);
	if (release) {
		Release();
	}
	return ret;
}

Local<Value> ExternalCopyHandle::Release() {
	CheckDisposed();
	Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-(ssize_t)value->OriginalSize());
	value.reset();
	return Undefined(Isolate::GetCurrent());
}

/**
 * ExternalCopyIntoHandle implementation
 */
ExternalCopyIntoHandle::ExternalCopyIntoTransferable::ExternalCopyIntoTransferable(shared_ptr<ExternalCopy> value, bool transfer_in) : value(std::move(value)), transfer_in(transfer_in) {}

Local<Value> ExternalCopyIntoHandle::ExternalCopyIntoTransferable::TransferIn() {
	return value->CopyIntoCheckHeap(transfer_in);
}

ExternalCopyIntoHandle::ExternalCopyIntoHandle(shared_ptr<ExternalCopy> value, bool transfer_in) : value(std::move(value)), transfer_in(transfer_in) {}

Local<FunctionTemplate> ExternalCopyIntoHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass("ExternalCopyInto", nullptr));
}

unique_ptr<Transferable> ExternalCopyIntoHandle::TransferOut() {
	if (!value) {
		throw js_generic_error("The return value of `copyInto()` should only be used once");
	}
	return std::make_unique<ExternalCopyIntoTransferable>(std::move(value), transfer_in);
}

} // namespace ivm
