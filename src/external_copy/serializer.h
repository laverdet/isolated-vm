#pragma once
#include "external_copy.h"
#include <deque>
#include <memory>
#include <vector>

#if !NODE_MODULE_OR_V8_AT_LEAST(72, 7, 5, 20)
namespace v8 {
	// Removed in 6b09d21c (v8), 963061bc (node)
	using WasmModuleObject = WasmCompiledModule;
}
#endif

namespace ivm {

/**
 * Serialized value from v8::ValueSerializer
 */
#if NODE_MODULE_OR_V8_AT_LEAST(83, 7, 9, 264)
#define USE_NEW_WASM 1
using CompiledWasmModuleHandle = v8::CompiledWasmModule;
#else
// deprecated: 6f838195, removed: 3bbadd00
// nodejs is also up to their usual hijinks of lying about v8 versions so nodejs v13.x which is
// supposedly on v8 7.9.317 doesn't support the new API
#define USE_NEW_WASM 0
using CompiledWasmModuleHandle = v8::WasmModuleObject::TransferrableModule;
#endif

class ExternalCopySerialized : public ExternalCopy {
	public:
		ExternalCopySerialized(v8::Local<v8::Object> value, ArrayRange transfer_list);
		auto CopyInto(bool transfer_in = false) -> v8::Local<v8::Value> final;

	private:
		std::unique_ptr<uint8_t, decltype(std::free)*> buffer = {nullptr, std::free};
		std::vector<std::unique_ptr<ExternalCopyArrayBuffer>> array_buffers;
		std::deque<std::unique_ptr<Transferable>> transferables;
		std::deque<CompiledWasmModuleHandle> wasm_modules;
		size_t size;
};

namespace detail {

class ExternalCopySerializationDelegateBase {
	public:
		ExternalCopySerializationDelegateBase(
			std::deque<std::unique_ptr<Transferable>>& transferables,
			std::deque<CompiledWasmModuleHandle>& wasm_modules
		) : transferables{transferables}, wasm_modules{wasm_modules} {}

	protected:
		std::deque<std::unique_ptr<Transferable>>& transferables;
		std::deque<CompiledWasmModuleHandle>& wasm_modules;
};

class ExternalCopySerializerDelegate : public ExternalCopySerializationDelegateBase, public v8::ValueSerializer::Delegate {
	public:
		using ExternalCopySerializationDelegateBase::ExternalCopySerializationDelegateBase;

		void SetSerializer(v8::ValueSerializer* serializer) { this->serializer = serializer; }
		void ThrowDataCloneError(v8::Local<v8::String> message) final;

		auto GetSharedArrayBufferId(
			v8::Isolate* isolate, v8::Local<v8::SharedArrayBuffer> shared_array_buffer) -> v8::Maybe<uint32_t> final;
		auto GetWasmModuleTransferId(
			v8::Isolate* isolate, v8::Local<v8::WasmModuleObject> module) -> v8::Maybe<uint32_t> final;
		auto WriteHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object) -> v8::Maybe<bool> final;

	private:
		v8::ValueSerializer* serializer = nullptr;
};

class ExternalCopyDeserializerDelegate : public ExternalCopySerializationDelegateBase, public v8::ValueDeserializer::Delegate {
	public:
		using ExternalCopySerializationDelegateBase::ExternalCopySerializationDelegateBase;

		void SetDeserializer(v8::ValueDeserializer* deserializer) { this->deserializer = deserializer; }

		auto GetSharedArrayBufferFromId(
			v8::Isolate* isolate, uint32_t clone_id) -> v8::MaybeLocal<v8::SharedArrayBuffer> final;
		auto GetWasmModuleFromId(
			v8::Isolate* isolate, uint32_t transfer_id) -> v8::MaybeLocal<v8::WasmModuleObject> final;
		auto ReadHostObject(v8::Isolate* isolate) -> v8::MaybeLocal<v8::Object> final;

	private:
		v8::ValueDeserializer* deserializer = nullptr;
};

} // namespace detail
} // namespace ivm
