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
	Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(this->value->Size());
}

ExternalCopyHandle::~ExternalCopyHandle() {
	if (value) {
		Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-(ssize_t)value->Size());
	}
}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& ExternalCopyHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> ExternalCopyHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"ExternalCopy", ParameterizeCtor<decltype(&New), &New>, 1,
		"copy", Parameterize<decltype(&ExternalCopyHandle::Copy), &ExternalCopyHandle::Copy>, 0,
		"copyInto", Parameterize<decltype(&ExternalCopyHandle::CopyInto), &ExternalCopyHandle::CopyInto>, 0,
		"dispose", Parameterize<decltype(&ExternalCopyHandle::Dispose), &ExternalCopyHandle::Dispose>, 0
	));
}

unique_ptr<Transferable> ExternalCopyHandle::TransferOut() {
	return std::make_unique<ExternalCopyTransferable>(value);
}

unique_ptr<ExternalCopyHandle> ExternalCopyHandle::New(Local<Value> value) {
	return std::make_unique<ExternalCopyHandle>(shared_ptr<ExternalCopy>(ExternalCopy::Copy(value)));
}

void ExternalCopyHandle::CheckDisposed() {
	if (!value) {
		throw js_generic_error("Copy is disposed");
	}
}

/**
 * JS API functions
 */
Local<Value> ExternalCopyHandle::Copy() {
	CheckDisposed();
	return value->CopyInto();
}

Local<Value> ExternalCopyHandle::CopyInto() {
	CheckDisposed();
	return ClassHandle::NewInstance<ExternalCopyIntoHandle>(value);
}

Local<Value> ExternalCopyHandle::Dispose() {
	CheckDisposed();
	Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-(ssize_t)value->Size());
	value.reset();
	return Undefined(Isolate::GetCurrent());
}

/**
 * ExternalCopyIntoHandle implementation
 */
ExternalCopyIntoHandle::ExternalCopyIntoTransferable::ExternalCopyIntoTransferable(shared_ptr<ExternalCopy> value) : value(std::move(value)) {}

Local<Value> ExternalCopyIntoHandle::ExternalCopyIntoTransferable::TransferIn() {
	return value->CopyInto();
}

ExternalCopyIntoHandle::ExternalCopyIntoHandle(shared_ptr<ExternalCopy> value) : value(std::move(value)) {}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& ExternalCopyIntoHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> ExternalCopyIntoHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass("ExternalCopyInto", nullptr, 0));
}

unique_ptr<Transferable> ExternalCopyIntoHandle::TransferOut() {
	return std::make_unique<ExternalCopyIntoTransferable>(value);
}

} // namespace ivm
