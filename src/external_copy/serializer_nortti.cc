#include "serializer.h"
#include "isolate/functor_runners.h"
#include "module/transferable.h"

/**
 * This file is compiled *without* runtime type information, which matches the nodejs binary and
 * allows the serializer delegates to resolve correctly.
 */

using namespace v8;

namespace ivm {
namespace detail {

void ExternalCopySerializerDelegate::ThrowDataCloneError(Local<String> message) {
	Isolate::GetCurrent()->ThrowException(Exception::TypeError(message));
}

auto ExternalCopySerializerDelegate::GetSharedArrayBufferId(
		Isolate* /*isolate*/, Local<SharedArrayBuffer> shared_array_buffer) -> Maybe<uint32_t> {
	auto result = Nothing<uint32_t>();
	detail::RunBarrier([&]() {
		transferables.emplace_back(std::make_unique<ExternalCopySharedArrayBuffer>(shared_array_buffer));
		result = Just<uint32_t>(transferables.size() - 1);
	});
	return result;
}

auto ExternalCopySerializerDelegate::GetWasmModuleTransferId(
		Isolate* /*isolate*/, Local<WasmModuleObject> module) -> Maybe<uint32_t> {
	auto result = Just<uint32_t>(wasm_modules.size());
#if USE_NEW_WASM
	wasm_modules.emplace_back(module->GetCompiledModule());
#else
	wasm_modules.emplace_back(module->GetTransferrableModule());
#endif
	return result;
}

auto ExternalCopySerializerDelegate::WriteHostObject(Isolate* /*isolate*/, Local<Object> object) -> Maybe<bool> {
	auto result = Nothing<bool>();
	detail::RunBarrier([&]() {
		serializer->WriteUint32(transferables.size());
		transferables.emplace_back(TransferOut(object));
		result = Just(true);
	});
	return result;
}

auto ExternalCopyDeserializerDelegate::ReadHostObject(Isolate* /*isolate*/) -> MaybeLocal<Object> {
	MaybeLocal<Object> result;
	detail::RunBarrier([&]() {
		uint32_t ii;
		assert(deserializer->ReadUint32(&ii));
		result = transferables[ii]->TransferIn().As<Object>();
	});
	return result;
}

auto ExternalCopyDeserializerDelegate::GetSharedArrayBufferFromId(
		Isolate* /*isolate*/, uint32_t clone_id) -> MaybeLocal<SharedArrayBuffer> {
	MaybeLocal<SharedArrayBuffer> result;
	detail::RunBarrier([&]() {
		result = transferables[clone_id]->TransferIn().As<SharedArrayBuffer>();
	});
	return result;
}

auto ExternalCopyDeserializerDelegate::GetWasmModuleFromId(
		Isolate* isolate, uint32_t transfer_id) -> MaybeLocal<WasmModuleObject> {
	MaybeLocal<WasmModuleObject> result;
	detail::RunBarrier([&]() {
#if USE_NEW_WASM
		result = WasmModuleObject::FromCompiledModule(isolate, wasm_modules[transfer_id]);
#else
		result = WasmModuleObject::FromTransferrableModule(isolate, wasm_modules[transfer_id]);
#endif
	});
	return result;
}

} // namespace detail
} // namespace ivm
