#include "external_copy.h"
#include "isolate/functor_runners.h"

using namespace v8;

namespace ivm {

/**
 * This file is compiled *without* runtime type information, which matches the nodejs binary and
 * allows the serializer delegates to resolve correctly.
 */
ExternalCopySerializerDelegate::ExternalCopySerializerDelegate(
	transferable_vector_t& references,
	shared_buffer_vector_t& shared_buffers
) :
	references(references), shared_buffers(shared_buffers) {}

void ExternalCopySerializerDelegate::ThrowDataCloneError(Local<String> message) {
	Isolate::GetCurrent()->ThrowException(Exception::TypeError(message));
}

Maybe<bool> ExternalCopySerializerDelegate::WriteHostObject(Isolate* /* isolate */, Local<Object> object) {
	Maybe<bool> result = Nothing<bool>();
	FunctorRunners::RunBarrier([&]() {
		references.emplace_back(Transferable::TransferOut(object));
		serializer->WriteUint32(references.size() - 1);
		result = Just(true);
	});
	return result;
}

Maybe<uint32_t> ExternalCopySerializerDelegate::GetSharedArrayBufferId(Isolate* /* isolate */, Local<SharedArrayBuffer> shared_array_buffer) {
	Maybe<uint32_t> result = Nothing<uint32_t>();
	FunctorRunners::RunBarrier([&]() {
		shared_buffers.emplace_back(std::make_unique<ExternalCopySharedArrayBuffer>(shared_array_buffer));
		result = Just<uint32_t>(shared_buffers.size() - 1);
	});
	return result;
}

ExternalCopyDeserializerDelegate::ExternalCopyDeserializerDelegate(transferable_vector_t& references) : references(references) {}

MaybeLocal<Object> ExternalCopyDeserializerDelegate::ReadHostObject(Isolate* /* isolate */) {
	MaybeLocal<Object> result;
	FunctorRunners::RunBarrier([&]() {
		uint32_t ii;
		assert(deserializer->ReadUint32(&ii));
		result = MaybeLocal<Object>(references[ii]->TransferIn().As<Object>());
	});
	return result;
}

} // namespace ivm
