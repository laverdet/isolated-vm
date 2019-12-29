#pragma once
#include <v8.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "isolate/transferable.h"
#include "isolate/util.h"

namespace ivm {

using handle_vector_t = std::vector<v8::Local<v8::Value>>;
using transferable_vector_t = std::vector<std::unique_ptr<Transferable>>;
using array_buffer_vector_t = std::vector<std::unique_ptr<class ExternalCopyArrayBuffer>>;
using shared_buffer_vector_t = std::vector<std::unique_ptr<class ExternalCopySharedArrayBuffer>>;

class ExternalCopySerializerDelegate : public v8::ValueSerializer::Delegate {
	private:
		transferable_vector_t& references;
		shared_buffer_vector_t& shared_buffers;

	public:
		v8::ValueSerializer* serializer = nullptr;
		ExternalCopySerializerDelegate(
			transferable_vector_t& references,
			shared_buffer_vector_t& shared_buffers
		);
		void ThrowDataCloneError(v8::Local<v8::String> message) final;
		v8::Maybe<bool> WriteHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object) final;
		v8::Maybe<uint32_t> GetSharedArrayBufferId(v8::Isolate* isolate, v8::Local<v8::SharedArrayBuffer> shared_array_buffer) final;
};

class ExternalCopyDeserializerDelegate : public v8::ValueDeserializer::Delegate {
	private:
		transferable_vector_t& references;

	public:
		v8::ValueDeserializer* deserializer = nullptr;
		explicit ExternalCopyDeserializerDelegate(transferable_vector_t& references);
		v8::MaybeLocal<v8::Object> ReadHostObject(v8::Isolate* isolate) final;
};

class ExternalCopy : public Transferable {
	public:
		ExternalCopy() = default;
		ExternalCopy(const ExternalCopy&) = delete;
		auto operator= (const ExternalCopy&) = delete;
		~ExternalCopy() override;

		/**
		 * `Copy` may throw a v8 exception if JSON.stringify(value) throws
		 */
		static std::unique_ptr<ExternalCopy> Copy(
			const v8::Local<v8::Value>& value,
			bool transfer_out = false,
			const handle_vector_t& transfer_list = handle_vector_t()
		);

		/**
		 * If you give this a primitive v8::Value (except Symbol) it will return a ExternalCopy for you.
		 * Otherwise it returns nullptr. This is used to automatically move simple values between
		 * isolates where it is possible to do so perfectly.
		 */
		static std::unique_ptr<ExternalCopy> CopyIfPrimitive(const v8::Local<v8::Value>& value);
		static std::unique_ptr<ExternalCopy> CopyIfPrimitiveOrError(const v8::Local<v8::Value>& value);

		static size_t TotalExternalSize();

		v8::Local<v8::Value> CopyIntoCheckHeap(bool transfer_in = false);
		virtual v8::Local<v8::Value> CopyInto(bool transfer_in = false) = 0;
		auto Size() const -> size_t { return size; }
		auto TransferIn() -> v8::Local<v8::Value> final;

	protected:
		explicit ExternalCopy(size_t size);
		ExternalCopy(ExternalCopy&& that) noexcept;
		auto operator= (ExternalCopy&& that) noexcept -> ExternalCopy&;

		void UpdateSize(size_t size);

	private:
		size_t size = 0;
};

/**
 * Serialized value from v8::ValueSerializer,
 */
class ExternalCopySerialized : public ExternalCopy {
	private:
		std::unique_ptr<uint8_t, decltype(std::free)*> buffer;
		size_t size;
		transferable_vector_t references;
		array_buffer_vector_t array_buffers;
		shared_buffer_vector_t shared_buffers;

	public:
		ExternalCopySerialized(
			std::pair<uint8_t*, size_t> val,
			transferable_vector_t references,
			array_buffer_vector_t array_buffers,
			shared_buffer_vector_t shared_buffers
		);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * null and undefined
 */
class ExternalCopyNull : public ExternalCopy {
	public:
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

class ExternalCopyUndefined : public ExternalCopy {
	public:
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * Identical to ExternalCopyTemplate except that Date uses `ValueOf` instead of `Value` -.-
 */
class ExternalCopyDate : public ExternalCopy {
	private:
		const double value;

	public:
		explicit ExternalCopyDate(const v8::Local<v8::Value>& value);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * Base class for ArrayBuffer and SharedArrayBuffer
 */
class ExternalCopyBytes : public ExternalCopy {
	protected:
		/**
		 * Holder is responsible for keeping a referenced to the shared_ptr around as long as the  JS
		 * instance is alive.
		 */
		struct Holder {
			static constexpr uint64_t kMagic = 0xa4d3c462f7fd1741;
			uint64_t magic = kMagic;
			v8::Persistent<v8::Object> v8_ptr;
			std::shared_ptr<void> cc_ptr;
			size_t size;

			Holder(const v8::Local<v8::Object>& buffer, std::shared_ptr<void> cc_ptr, size_t size);
			Holder(const Holder&) = delete;
			Holder& operator= (const Holder&) = delete;
			~Holder();
			static void WeakCallbackV8(const v8::WeakCallbackInfo<void>& info);
			static void WeakCallback(void* param);
		};

		std::shared_ptr<void> Release();
		void Replace(std::shared_ptr<void> value);

	public:
		explicit ExternalCopyBytes(size_t size, std::shared_ptr<void> value, size_t length);
		std::shared_ptr<void> Acquire() const;
		size_t Length() { return length; }

	private:
		std::shared_ptr<void> value;
		const size_t length;
		mutable std::mutex mutex;
};

/**
 * ArrayBuffer instances
 */
class ExternalCopyArrayBuffer : public ExternalCopyBytes {
	public:
		ExternalCopyArrayBuffer(const void* data, size_t length);
		ExternalCopyArrayBuffer(std::shared_ptr<void> ptr, size_t length);
		explicit ExternalCopyArrayBuffer(const v8::Local<v8::ArrayBuffer>& handle);

		static std::unique_ptr<ExternalCopyArrayBuffer> Transfer(const v8::Local<v8::ArrayBuffer>& handle);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * SharedArrayBuffer instances
 */
class ExternalCopySharedArrayBuffer : public ExternalCopyBytes {
	public:
		explicit ExternalCopySharedArrayBuffer(const v8::Local<v8::SharedArrayBuffer>& handle);

		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * All types of TypedArray views w/ underlying buffer handle
 */
class ExternalCopyArrayBufferView : public ExternalCopy {
	public:
		enum class ViewType { Uint8, Uint8Clamped, Int8, Uint16, Int16, Uint32, Int32, Float32, Float64, DataView };

	private:
		std::unique_ptr<ExternalCopyBytes> buffer;
		ViewType type;
		size_t byte_offset, byte_length;

	public:
		ExternalCopyArrayBufferView(std::unique_ptr<ExternalCopyBytes> buffer, ViewType type, size_t byte_offset, size_t byte_length);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

} // namespace ivm
