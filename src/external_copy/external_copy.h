#pragma once
#include <v8.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "isolate/generic/array.h"
#include "isolate/transferable.h"
#include "isolate/util.h"

namespace ivm {

using handle_vector_t = std::vector<v8::Local<v8::Value>>;
using transferable_vector_t = std::vector<std::unique_ptr<Transferable>>;
using array_buffer_vector_t = std::vector<std::unique_ptr<class ExternalCopyArrayBuffer>>;
using shared_buffer_vector_t = std::vector<std::unique_ptr<class ExternalCopySharedArrayBuffer>>;

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
			v8::Local<v8::Value> value,
			bool transfer_out = false,
			ArrayRange transfer_list = {}
		);

		/**
		 * If you give this a primitive v8::Value (except Symbol) it will return a ExternalCopy for you.
		 * Otherwise it returns nullptr. This is used to automatically move simple values between
		 * isolates where it is possible to do so perfectly.
		 */
		static auto CopyIfPrimitive(v8::Local<v8::Value> value) -> std::unique_ptr<ExternalCopy>;
		static auto CopyThrownValue(v8::Local<v8::Value> value) -> std::unique_ptr<ExternalCopy>;

		static auto TotalExternalSize() -> int;

		v8::Local<v8::Value> CopyIntoCheckHeap(bool transfer_in = false);
		virtual v8::Local<v8::Value> CopyInto(bool transfer_in = false) = 0;
		auto Size() const -> int { return size; }
		auto TransferIn() -> v8::Local<v8::Value> final { return CopyIntoCheckHeap(); }

	protected:
		explicit ExternalCopy(int size);
		ExternalCopy(ExternalCopy&& that) noexcept;
		auto operator= (ExternalCopy&& that) noexcept -> ExternalCopy&;

		void UpdateSize(int size);

	private:
		int size = 0;
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

class ExternalCopyAnyArrayBuffer {
	public:
		virtual ~ExternalCopyAnyArrayBuffer() = default;
		virtual v8::Local<v8::Value> CopyInto(bool transfer_in = false) = 0;
};

/**
 * ArrayBuffer instances
 */
class ExternalCopyArrayBuffer : public ExternalCopyBytes, public ExternalCopyAnyArrayBuffer {
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
#if V8_AT_LEAST(7, 9, 69)
class ExternalCopySharedArrayBuffer : public ExternalCopy, public ExternalCopyAnyArrayBuffer {
	private:
		std::shared_ptr<v8::BackingStore> backing_store;
#else
class ExternalCopySharedArrayBuffer : public ExternalCopyBytes, public ExternalCopyAnyArrayBuffer {
#endif
	public:
		explicit ExternalCopySharedArrayBuffer(const v8::Local<v8::SharedArrayBuffer>& handle);

		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

/**
 * All types of TypedArray views w/ underlying buffer handle
 */
class ExternalCopyArrayBufferView : public ExternalCopy {
	public:
		enum class ViewType { Uint8, Uint8Clamped, Int8, Uint16, Int16, Uint32, Int32, Float32, Float64, BigInt64Array, BigUint64Array, DataView };

	private:
		std::unique_ptr<ExternalCopyAnyArrayBuffer> buffer;
		ViewType type;
		size_t byte_offset, byte_length;

	public:
		ExternalCopyArrayBufferView(std::unique_ptr<ExternalCopyAnyArrayBuffer> buffer, ViewType type, size_t byte_offset, size_t byte_length);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
};

} // namespace ivm
