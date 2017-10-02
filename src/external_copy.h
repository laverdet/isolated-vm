#pragma once
#include <node.h>
#include <assert.h>
#include <stdint.h>
#include "util.h"
#include "transferable.h"

#include <memory>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ivm {
using namespace v8;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

class ExternalCopy : public Transferable {
	public:
		virtual ~ExternalCopy() {}
		virtual Local<Value> CopyInto() const = 0;
		virtual size_t Size() const = 0;

		/**
		 * `Copy` may throw a v8 exception if JSON.stringify(value) throws
		 */
		static unique_ptr<ExternalCopy> Copy(const Local<Value>& value);

		/**
		 * If you give this a primitive v8::Value (except Symbol) it will return a ExternalCopy for you.
		 * Otherwise it returns nullptr. This is used to automatically move simple values between
		 * isolates where it is possible to do so perfectly.
		 */
		static unique_ptr<ExternalCopy> CopyIfPrimitive(const Local<Value>& value);
		static unique_ptr<ExternalCopy> CopyIfPrimitiveOrError(const Local<Value>& value);

		Local<Value> TransferIn() {
			return CopyInto();
		}
};

/**
 * This will make a copy of any Number (several C++ types), or Boolean. Strings are handled
 * by the specialization below.
 */
template <typename T, typename V>
class ExternalCopyTemplate : public ExternalCopy {
	private:
		const V value;

	public:
		ExternalCopyTemplate(const Local<Value>& value) : value(Local<T>::Cast(value)->Value()) {}

		virtual Local<Value> CopyInto() const {
			return T::New(Isolate::GetCurrent(), value);
		}

		virtual size_t Size() const {
			return sizeof(V);
		}
};

/**
 * String specialization for ExternalCopyTemplate.
 */
template <>
class ExternalCopyTemplate<String, shared_ptr<vector<uint16_t>>> : public ExternalCopy {
	private:
		// shared_ptr<> to share external strings between isolates
		shared_ptr<vector<uint16_t>> value;

		/**
		 * Helper class passed to v8 so we can reuse the same externally allocated memory for strings
		 * between different isolates
		 */
		class ExternalString : public String::ExternalStringResource {
			private:
				shared_ptr<vector<uint16_t>> value;

			public:
				ExternalString(shared_ptr<vector<uint16_t>> value) : value(value) {}

				virtual const uint16_t* data() const {
					return &(*value)[0];
				}

				virtual size_t length() const {
					return value->size();
				}
		};

	public:
		ExternalCopyTemplate(const Local<Value>& value) {
			String::Value v8_string(Local<String>::Cast(value));
			this->value = std::make_shared<vector<uint16_t>>(*v8_string, *v8_string + v8_string.length());
		}

		virtual Local<Value> CopyInto() const {
			if (value->size()) {
				return String::NewExternalTwoByte(Isolate::GetCurrent(), new ExternalString(value)).ToLocalChecked();
			} else {
				// v8 crashes if you send it an empty external string :(
				return String::Empty(Isolate::GetCurrent());
			}
		}

		virtual size_t Size() const {
			return value->size() * sizeof(uint16_t);
		}
};

typedef ExternalCopyTemplate<String, shared_ptr<vector<uint16_t>>> ExternalCopyString;

/**
 * Just a JSON blob that will be automatically parsed.
 */
class ExternalCopyJSON : public ExternalCopy {
	private:
		ExternalCopyString blob;

	public:
		// `value` here should be an already JSON'd string
		ExternalCopyJSON(Local<Value> value) : blob(value) {}

		virtual Local<Value> CopyInto() const {
			return JSON::Parse(Isolate::GetCurrent()->GetCurrentContext(), Local<String>::Cast(blob.CopyInto())).ToLocalChecked();
		}

		virtual size_t Size() const {
			return blob.Size();
		}
};

/**
 * Make a special case for errors so if someone throws then a similar error will come out the other
 * side.
 */
class ExternalCopyError : public ExternalCopy {
	friend class ExternalCopy;
	private:
		enum class ErrorType { RangeError = 1, ReferenceError, SyntaxError, TypeError, Error };
		ErrorType error_type;
		unique_ptr<ExternalCopyString> message;
		unique_ptr<ExternalCopyString> stack;

	public:
		ExternalCopyError(ErrorType error_type, unique_ptr<ExternalCopyString> message, unique_ptr<ExternalCopyString> stack) : error_type(error_type), message(std::move(message)), stack(std::move(stack)) {}
		virtual Local<Value> CopyInto() const {

			// First make the exception w/ correct + message
			Local<String> message(Local<String>::Cast(this->message->CopyInto()));
			Local<Value> handle;
			switch (error_type) {
				case ErrorType::RangeError:
					handle = Exception::RangeError(message);
					break;
				case ErrorType::ReferenceError:
					handle = Exception::ReferenceError(message);
					break;
				case ErrorType::SyntaxError:
					handle = Exception::SyntaxError(message);
					break;
				case ErrorType::TypeError:
					handle = Exception::TypeError(message);
					break;
				case ErrorType::Error:
					handle = Exception::Error(message);
					break;
			}

			// Now add stack information
			if (this->stack.get() != nullptr) {
				Local<String> stack(Local<String>::Cast(this->stack->CopyInto()));
				Local<Object>::Cast(handle)->Set(v8_symbol("stack"), stack);
			}
			return handle;
		}

		virtual size_t Size() const {
			return
				(message.get() == nullptr ? 0 : message->Size()) +
				(stack.get() == nullptr ? 0 : stack->Size());
		}
};

/**
 * null and undefined
 */
class ExternalCopyNull : public ExternalCopy {
	public:
		virtual Local<Value> CopyInto() const {
			return Null(Isolate::GetCurrent());
		}

		virtual size_t Size() const {
			return 0;
		}
};

class ExternalCopyUndefined : public ExternalCopy {
	public:
		virtual Local<Value> CopyInto() const {
			return Undefined(Isolate::GetCurrent());
		}

		virtual size_t Size() const {
			return 0;
		}
};

/**
 * Identical to ExternalCopyTemplate except that v8::Date uses `ValueOf` instead of `Value` -.-
 */
class ExternalCopyDate : public ExternalCopy {
	private:
		const double value;

	public:
		ExternalCopyDate(const Local<Value>& value) : value(Local<Date>::Cast(value)->ValueOf()) {}

		virtual Local<Value> CopyInto() const {
			return Date::New(Isolate::GetCurrent(), value);
		}

		virtual size_t Size() const {
			return sizeof(double);
		}
};

/**
 * ArrayBuffer, just the raw bytes part.
 */
class ExternalCopyArrayBuffer : public ExternalCopy {
	private:
		unique_ptr<void, decltype(std::free)*> value;
		const size_t length;

	public:
		ExternalCopyArrayBuffer(const void* data, size_t length) : value(malloc(length), std::free), length(length) {
			std::memcpy(value.get(), data, length);
		}

		ExternalCopyArrayBuffer(Local<ArrayBufferView>& handle) : value(malloc(handle->ByteLength()), std::free), length(handle->ByteLength()) {
			handle->CopyContents(value.get(), length);
		}

		virtual Local<Value> CopyInto() const {
			Local<ArrayBuffer> array_buffer(ArrayBuffer::New(Isolate::GetCurrent(), length));
			std::memcpy(array_buffer->GetContents().Data(), value.get(), length);
			return array_buffer;
		}

		virtual size_t Size() const {
			return length;
		}

		const void* Data() const {
			return value.get();
		}

		size_t Length() const {
			return length;
		}
};

/**
 * All types of ArrayBuffer views
 */
class ExternalCopyArrayBufferView : public ExternalCopy {
	public:
		enum class ViewType { Uint8, Uint8Clamped, Int8, Uint16, Int16, Uint32, Int32, Float32, Float64, DataView };

	private:
		ExternalCopyArrayBuffer buffer;
		ViewType type;

	public:
		ExternalCopyArrayBufferView(Local<ArrayBufferView>& handle, ViewType type) : buffer(handle), type(type) {}

		virtual Local<Value> CopyInto() const {
			Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(this->buffer.CopyInto());
			size_t length = buffer->ByteLength();
			switch (type) {
				case ViewType::Uint8:
					return Uint8Array::New(buffer, 0, length);
				case ViewType::Uint8Clamped:
					return Uint8ClampedArray::New(buffer, 0, length);
				case ViewType::Int8:
					return Int8Array::New(buffer, 0, length);
				case ViewType::Uint16:
					return Uint16Array::New(buffer, 0, length);
				case ViewType::Int16:
					return Int16Array::New(buffer, 0, length);
				case ViewType::Uint32:
					return Uint32Array::New(buffer, 0, length);
				case ViewType::Int32:
					return Int32Array::New(buffer, 0, length);
				case ViewType::Float32:
					return Float32Array::New(buffer, 0, length);
				case ViewType::Float64:
					return Float64Array::New(buffer, 0, length);
				case ViewType::DataView:
					return DataView::New(buffer, 0, length);
				default:
					throw std::exception();
			}
		}

		virtual size_t Size() const {
			return buffer.Size();
		}
};

}
