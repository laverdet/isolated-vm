#include "serializer.h"
#include "isolate/functor_runners.h"
#include "module/transferable.h"
#include "isolate/generic/object.h"

/**
 * This file is compiled *without* runtime type information, which matches the nodejs binary and
 * allows the serializer delegates to resolve correctly.
 */

using namespace v8;

namespace ivm::detail {

void SerializerDelegate::ThrowDataCloneError(Local<String> message) {
	Isolate::GetCurrent()->ThrowException(Exception::TypeError(message));
}

auto SerializerDelegate::GetSharedArrayBufferId(
		Isolate* /*isolate*/, Local<SharedArrayBuffer> shared_array_buffer) -> Maybe<uint32_t> {
	auto result = Nothing<uint32_t>();
	detail::RunBarrier([&]() {
		transferables.emplace_back(std::make_unique<ExternalCopySharedArrayBuffer>(shared_array_buffer));
		result = Just<uint32_t>(transferables.size() - 1);
	});
	return result;
}

auto SerializerDelegate::GetWasmModuleTransferId(
		Isolate* /*isolate*/, Local<WasmModuleObject> module) -> Maybe<uint32_t> {
	auto result = Just<uint32_t>(wasm_modules.size());
	wasm_modules.emplace_back(module->GetCompiledModule());
	return result;
}

auto SerializerDelegate::WriteHostObject(Isolate* isolate, Local<Object> object) -> Maybe<bool> {
	auto result = Nothing<bool>();
	detail::RunBarrier([&]() {
		auto host_object_id = transferables.size();
		serializer->WriteUint32(host_object_id);

		if (object->IsArrayBufferView()) {
			auto view = object.As<ArrayBufferView>();
			transferables.emplace_back(ExternalCopyArrayBufferView::Copy(view));
			array_buffer_view_indexes.emplace_back(host_object_id);
			result = WriteArrayBufferView(isolate, view);
		} else {
			transferables.emplace_back(TransferOut(object));
			result = Just(true);
		}
	});
	return result;
}

auto SerializerDelegate::WriteArrayBufferView(v8::Isolate* isolate, v8::Local<v8::ArrayBufferView> view) -> v8::Maybe<bool> {
	Local<Context> context = isolate->GetCurrentContext();
	auto result = serializer->WriteValue(context, view->Buffer());

	if (result.IsJust()) {
		auto properties = ExternalCopyArrayBufferView::CopyOwnProperties(isolate, view);
		return serializer->WriteValue(context, properties);
	}

	return result;
}

auto DeserializerDelegate::ReadHostObject(Isolate* isolate) -> MaybeLocal<Object> {
	MaybeLocal<Object> result;
	detail::RunBarrier([&]() {
		uint32_t ii;
		assert(deserializer->ReadUint32(&ii));

		// Replacing std::find with a for loop for compatibility with Alpine Linux
		for (const auto& value : array_buffer_view_indexes) {
			if (value == ii) {
				ReadArrayBufferView(isolate, ii);
				break;
			}
		}

		result = transferables[ii]->TransferIn().As<Object>();
	});
	return result;
}

auto DeserializerDelegate::GetSharedArrayBufferFromId(
		Isolate* /*isolate*/, uint32_t clone_id) -> MaybeLocal<SharedArrayBuffer> {
	MaybeLocal<SharedArrayBuffer> result;
	detail::RunBarrier([&]() {
		result = transferables[clone_id]->TransferIn().As<SharedArrayBuffer>();
	});
	return result;
}

auto DeserializerDelegate::GetWasmModuleFromId(
		Isolate* isolate, uint32_t transfer_id) -> MaybeLocal<WasmModuleObject> {
	MaybeLocal<WasmModuleObject> result;
	detail::RunBarrier([&]() {
		result = WasmModuleObject::FromCompiledModule(isolate, wasm_modules[transfer_id]);
	});
	return result;
}

auto DeserializerDelegate::ReadArrayBufferView(Isolate* isolate, uint32_t transferable_id) -> void {
	auto *view_copy = reinterpret_cast<std::unique_ptr<ExternalCopyArrayBufferView>*>(&transferables[transferable_id])->get();
	Local<Context> context = isolate->GetCurrentContext();
    auto underlying_buffer = deserializer->ReadValue(context).ToLocalChecked().As<Object>();
	auto properties = deserializer->ReadValue(context).ToLocalChecked().As<Object>();

	view_copy->SetUnderlyingBuffer(underlying_buffer);
	view_copy->SetOwnProperties(properties);
}

} // namespace ivm::detail
