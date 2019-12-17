#pragma once
#include <v8.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "transferable.h"
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
	private:
		size_t size = 0;
		size_t original_size = 0;
		static std::atomic<size_t> total_allocated_size;

	public:
		ExternalCopy();
		explicit ExternalCopy(size_t size);
		ExternalCopy(const ExternalCopy&) = delete;
		ExternalCopy& operator= (const ExternalCopy&) = delete;
		ExternalCopy(ExternalCopy&& that);
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
		void UpdateSize(size_t size);
		size_t OriginalSize() const;
		v8::Local<v8::Value> TransferIn() final;
};

/**
 * This will make a copy of any Number (several C++ types), or Boolean. Strings are handled
 * by the specialization below.
 */
template <typename Type>
struct ExternalCopyTemplateCtor {
	template <typename Value>
	static v8::Local<v8::Value> New(v8::Isolate* isolate, Value value) {
		return Type::New(isolate, value);
	}
};

template <>
struct ExternalCopyTemplateCtor<v8::Uint32> {
	static v8::Local<v8::Value> New(v8::Isolate* isolate, uint32_t value) {
		return v8::Uint32::NewFromUnsigned(isolate, value);
	}
};

template <typename T, typename V>
class ExternalCopyTemplate : public ExternalCopy {
	private:
		const V value;

	public:
		explicit ExternalCopyTemplate(const v8::Local<v8::Value>& value) : value(v8::Local<T>::Cast(value)->Value()) {}

		v8::Local<v8::Value> CopyInto(bool /*transfer_in*/ = false) final {
			return ExternalCopyTemplateCtor<T>::New(v8::Isolate::GetCurrent(), value);
		}
};

/**
 * String data
 */
class ExternalCopyString : public ExternalCopy {
	private:
		// shared_ptr<> to share external strings between isolates
		using V = std::vector<char>;
		bool one_byte;
		std::shared_ptr<V> value;

		/**
		 * Helper class passed to v8 so we can reuse the same externally allocated memory for strings
		 * between different isolates
		 */
		class ExternalString : public v8::String::ExternalStringResource {
			private:
				std::shared_ptr<V> value;

			public:
				explicit ExternalString(std::shared_ptr<V> value);
				ExternalString(const ExternalString&) = delete;
				ExternalString& operator= (const ExternalString&) = delete;
				~ExternalString() final;
				const uint16_t* data() const final;
				size_t length() const final;
		};

		class ExternalStringOneByte : public v8::String::ExternalOneByteStringResource {
			private:
				std::shared_ptr<V> value;

			public:
				explicit ExternalStringOneByte(std::shared_ptr<V> value);
				ExternalStringOneByte(const ExternalStringOneByte&) = delete;
				ExternalStringOneByte& operator= (const ExternalStringOneByte&) = delete;
				~ExternalStringOneByte() final;
				const char* data() const final;
				size_t length() const final;
		};

	public:
		// gcc 5 doesn't want this to be explicit
		ExternalCopyString(v8::Local<v8::String> string);
		explicit ExternalCopyString(const char* message);
		explicit ExternalCopyString(const std::string& message);
		v8::Local<v8::Value> CopyInto(bool transfer_in = false) final;
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
 * Make a special case for errors so if someone throws then a similar error will come out the other
 * side.
 */
class ExternalCopyError : public ExternalCopy {
	friend class ExternalCopy;
	public:
		enum class ErrorType { Error, RangeError, ReferenceError, SyntaxError, TypeError, CustomError };

	private:
		ErrorType error_type;
		// these could be std::optional
		std::unique_ptr<ExternalCopyString> name;
		std::unique_ptr<ExternalCopyString> message;
		std::unique_ptr<ExternalCopyString> stack;

	public:
		ExternalCopyError(
			ErrorType error_type,
			std::unique_ptr<ExternalCopyString> name,
			std::unique_ptr<ExternalCopyString> message,
			std::unique_ptr<ExternalCopyString> stack
		);
		ExternalCopyError(ErrorType error_type, const char* message, std::string stack = "");
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
