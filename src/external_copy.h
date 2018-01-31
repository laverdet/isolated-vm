#pragma once
#include <v8.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "transferable.h"
#include "isolate/util.h"

namespace ivm {

class ExternalCopy : public Transferable {
	public:
		/**
		 * `Copy` may throw a v8 exception if JSON.stringify(value) throws
		 */
		static std::unique_ptr<ExternalCopy> Copy(const v8::Local<v8::Value>& value);

		/**
		 * If you give this a primitive v8::Value (except Symbol) it will return a ExternalCopy for you.
		 * Otherwise it returns nullptr. This is used to automatically move simple values between
		 * isolates where it is possible to do so perfectly.
		 */
		static std::unique_ptr<ExternalCopy> CopyIfPrimitive(const v8::Local<v8::Value>& value);
		static std::unique_ptr<ExternalCopy> CopyIfPrimitiveOrError(const v8::Local<v8::Value>& value);

		virtual v8::Local<v8::Value> CopyInto() const = 0;
		virtual size_t Size() const = 0;
		v8::Local<v8::Value> TransferIn() final;
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
		explicit ExternalCopyTemplate(const v8::Local<v8::Value>& value) : value(v8::Local<T>::Cast(value)->Value()) {}

		v8::Local<v8::Value> CopyInto() const final {
			return T::New(v8::Isolate::GetCurrent(), value);
		}

		size_t Size() const final {
			return sizeof(V);
		}
};

/**
 * String specialization for ExternalCopyTemplate.
 */
template <>
class ExternalCopyTemplate<v8::String, std::shared_ptr<std::vector<uint16_t>>> : public ExternalCopy {
	private:
		// shared_ptr<> to share external strings between isolates
		using V = std::vector<uint16_t>;
		std::shared_ptr<V> value;

		/**
		 * Helper class passed to v8 so we can reuse the same externally allocated memory for strings
		 * between different isolates
		 */
		class ExternalString : public v8::String::ExternalStringResource {
			private:
				std::shared_ptr<V> value;

			public:
				explicit ExternalString(std::shared_ptr<V> value) : value(std::move(value)) {}

				const uint16_t* data() const final {
					return &(*value)[0];
				}

				size_t length() const final {
					return value->size();
				}
		};

	public:
		explicit ExternalCopyTemplate(const v8::Local<v8::Value>& value) {
			v8::String::Value v8_string(v8::Local<v8::String>::Cast(value));
			this->value = std::make_shared<V>(*v8_string, *v8_string + v8_string.length());
		}

		explicit ExternalCopyTemplate(const std::string& message) : value(std::make_shared<V>(message.begin(), message.end())) {}

		v8::Local<v8::Value> CopyInto() const final {
			if (value->empty()) {
				// v8 crashes if you send it an empty external string :(
				return v8::String::Empty(v8::Isolate::GetCurrent());
			} else {
				return Unmaybe(v8::String::NewExternalTwoByte(v8::Isolate::GetCurrent(), new ExternalString(value)));
			}
		}

		size_t Size() const final {
			return value->size() * sizeof(uint16_t);
		}
};

using ExternalCopyString = ExternalCopyTemplate<v8::String, std::shared_ptr<std::vector<uint16_t>>>;

/**
 * Just a JSON blob that will be automatically parsed.
 */
class ExternalCopyJSON : public ExternalCopy {
	private:
		ExternalCopyString blob;

	public:
		// `value` here should be an already JSON'd string
		explicit ExternalCopyJSON(const v8::Local<v8::Value>& value);
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
};

/**
 * Make a special case for errors so if someone throws then a similar error will come out the other
 * side.
 */
class ExternalCopyError : public ExternalCopy {
	friend class ExternalCopy;
	public:
		enum class ErrorType { RangeError = 1, ReferenceError, SyntaxError, TypeError, Error };

	private:
		ErrorType error_type;
		std::unique_ptr<ExternalCopyString> message;
		std::unique_ptr<ExternalCopyString> stack;

	public:
		ExternalCopyError(ErrorType error_type, std::unique_ptr<ExternalCopyString> message, std::unique_ptr<ExternalCopyString> stack);
		ExternalCopyError(ErrorType error_type, const std::string& message);
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
};

/**
 * null and undefined
 */
class ExternalCopyNull : public ExternalCopy {
	public:
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
};

class ExternalCopyUndefined : public ExternalCopy {
	public:
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
};

/**
 * Identical to ExternalCopyTemplate except that Date uses `ValueOf` instead of `Value` -.-
 */
class ExternalCopyDate : public ExternalCopy {
	private:
		const double value;

	public:
		explicit ExternalCopyDate(const v8::Local<v8::Value>& value);
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
};

/**
 * ArrayBuffer, just the raw bytes part.
 */
class ExternalCopyArrayBuffer : public ExternalCopy {
	private:
		std::unique_ptr<void, decltype(std::free)*> value;
		const size_t length;

	public:
		ExternalCopyArrayBuffer(const void* data, size_t length);
		explicit ExternalCopyArrayBuffer(const v8::Local<v8::ArrayBufferView>& handle);
		v8::Local<v8::Value> CopyInto() const final;
		size_t Size() const final;
		const void* Data() const;
		size_t Length() const;
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
		ExternalCopyArrayBufferView(const v8::Local<v8::ArrayBufferView>& handle, ViewType type);
		v8::Local<v8::Value> CopyInto() const override;
		size_t Size() const override;
};

} // namespace ivm
