#include "external_copy.h"

#include "util.h"

#include <cstring>

using namespace v8;
using std::make_unique;
using std::unique_ptr;

namespace ivm {

/**
 * ExternalCopy implemtation
 */
unique_ptr<ExternalCopy> ExternalCopy::Copy(const Local<Value>& value) {
	unique_ptr<ExternalCopy> copy = CopyIfPrimitive(value);
	if (copy) {
		return copy;
	} else if (value->IsDate()) {
		return make_unique<ExternalCopyDate>(value);
	} else if (value->IsArrayBuffer()) {
		Local<ArrayBuffer> array_buffer(Local<ArrayBuffer>::Cast(value));
		ArrayBuffer::Contents contents(array_buffer->GetContents());
		return make_unique<ExternalCopyArrayBuffer>(contents.Data(), contents.ByteLength());
	} else if (value->IsArrayBufferView()) {
		Local<ArrayBufferView> view(Local<ArrayBufferView>::Cast(value));
		assert(view->HasBuffer());
		using ViewType = ExternalCopyArrayBufferView::ViewType;
		ViewType type;
		if (view->IsUint8Array()) {
			type = ViewType::Uint8;
		} else if (view->IsUint8ClampedArray()) {
			type = ViewType::Uint8Clamped;
		} else if (view->IsInt8Array()) {
			type = ViewType::Int8;
		} else if (view->IsUint16Array()) {
			type = ViewType::Uint16;
		} else if (view->IsInt16Array()) {
			type = ViewType::Int16;
		} else if (view->IsUint32Array()) {
			type = ViewType::Uint32;
		} else if (view->IsInt32Array()) {
			type = ViewType::Int32;
		} else if (view->IsFloat32Array()) {
			type = ViewType::Float32;
		} else if (view->IsFloat64Array()) {
			type = ViewType::Float64;
		} else if (view->IsDataView()) {
			type = ViewType::DataView;
		} else {
			assert(false);
		}
		return make_unique<ExternalCopyArrayBufferView>(view, type);
	} else if (value->IsObject()) {
		Local<String> string = Unmaybe(JSON::Stringify(Isolate::GetCurrent()->GetCurrentContext(), Local<Object>::Cast(value)));
		if (string->StrictEquals(v8_symbol("undefined"))) {
			// v8 will return the string literal 'undefined' if there was a non-throw exception, like
			// JSON.stringify(undefined). So we have to specifically check for that because 'undefined'
			// isn't valid JSON
			throw js_type_error("Could not JSON.stringify() object for an external copy");
		}
		return make_unique<ExternalCopyJSON>(string);
	} else {
		// ???
		assert(false);
		throw std::exception();
	}
}

unique_ptr<ExternalCopy> ExternalCopy::CopyIfPrimitive(const Local<Value>& value) {
	if (value->IsBoolean()) {
		return make_unique<ExternalCopyTemplate<Boolean, bool>>(value);
	} else if (value->IsNumber()) {
		if (value->IsUint32()) {
			return make_unique<ExternalCopyTemplate<Uint32, uint32_t>>(value);
		} else if (value->IsInt32()) {
			return make_unique<ExternalCopyTemplate<Int32, int32_t>>(value);
		} else {
			// This handles Infinity, -Infinity, NaN
			return make_unique<ExternalCopyTemplate<Number, double>>(value);
		}
	} else if (value->IsString()) {
		return make_unique<ExternalCopyString>(value);
	} else if (value->IsNull()) {
		return make_unique<ExternalCopyNull>();
	} else if (value->IsUndefined() || value->IsSymbol()) {
		return make_unique<ExternalCopyUndefined>();
	} else {
		return nullptr;
	}
}

unique_ptr<ExternalCopy> ExternalCopy::CopyIfPrimitiveOrError(const Local<Value>& value) {
	if (value->IsObject()) {

		// Detect which subclass of Error was thrown (no better way to do this??)
		Local<Object> object(Local<Object>::Cast(value));
		std::string name(*String::Utf8Value(object->GetConstructorName()));
		ExternalCopyError::ErrorType error_type = (ExternalCopyError::ErrorType)0;
		if (name == "RangeError") {
			error_type = ExternalCopyError::ErrorType::RangeError;
		} else if (name == "ReferenceError") {
			error_type = ExternalCopyError::ErrorType::ReferenceError;
		} else if (name == "SyntaxError") {
			error_type = ExternalCopyError::ErrorType::SyntaxError;
		} else if (name == "TypeError") {
			error_type = ExternalCopyError::ErrorType::TypeError;
		} else if (name == "Error") {
			error_type = ExternalCopyError::ErrorType::Error;
		}

		// If we matched one of the Error constructors will copy this object
		if (error_type != (ExternalCopyError::ErrorType)0) {

			// Get `message`
			TryCatch try_catch(Isolate::GetCurrent());
			Local<Value> message(object->Get(v8_symbol("message")));
			unique_ptr<ExternalCopyString> message_copy;
			if (try_catch.HasCaught()) {
				try_catch.Reset();
			} else if (message->IsString()) {
				message_copy = make_unique<ExternalCopyString>(message);
			} else {
				message_copy = make_unique<ExternalCopyString>(v8_string(""));
			}

			// Get `stack`
			Local<Value> stack(object->Get(v8_symbol("stack")));
			unique_ptr<ExternalCopyString> stack_copy;
			if (try_catch.HasCaught()) {
				// printf("try_catch: %s<<\n", *String::Utf8Value(try_catch.Message()->Get()));
				try_catch.Reset();
			} else if (stack->IsString()) {
				stack_copy = make_unique<ExternalCopyString>(stack);
			}

			return make_unique<ExternalCopyError>(error_type, std::move(message_copy), std::move(stack_copy));
		}
	}
	return CopyIfPrimitive(value);
}

Local<Value> ExternalCopy::TransferIn() {
	return CopyInto();
}

/**
 * ExternalCopyJSON implemtation
 */
ExternalCopyJSON::ExternalCopyJSON(const Local<Value>& value) : blob(value) {}

Local<Value> ExternalCopyJSON::CopyInto() const {
	return Unmaybe(JSON::Parse(Isolate::GetCurrent()->GetCurrentContext(), Local<String>::Cast(blob.CopyInto())));
}

size_t ExternalCopyJSON::Size() const {
	return blob.Size();
}

/**
 * ExternalCopyError implemtation
 */
ExternalCopyError::ExternalCopyError(
	ErrorType error_type,
	unique_ptr<ExternalCopyString> message,
	unique_ptr<ExternalCopyString> stack)
	: error_type(error_type),
	message(std::move(message)),
	stack(std::move(stack)) {}

ExternalCopyError::ExternalCopyError(ErrorType error_type, const std::string& message) : error_type(error_type), message(make_unique<ExternalCopyString>(message)) {}

Local<Value> ExternalCopyError::CopyInto() const {

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
	if (this->stack) {
		Local<String> stack(Local<String>::Cast(this->stack->CopyInto()));
		Local<Object>::Cast(handle)->Set(v8_symbol("stack"), stack);
	}
	return handle;
}

size_t ExternalCopyError::Size() const {
	return
		(message ? message->Size() : 0) +
		(stack ? stack->Size() : 0);
}

/**
 * ExternalCopyNull and ExternalCopyUndefined implemtations
 */
Local<Value> ExternalCopyNull::CopyInto() const {
	return Null(Isolate::GetCurrent());
}

size_t ExternalCopyNull::Size() const {
	return 0;
}

Local<Value> ExternalCopyUndefined::CopyInto() const {
	return Undefined(Isolate::GetCurrent());
}

size_t ExternalCopyUndefined::Size() const {
	return 0;
}

/**
 * ExternalCopyDate implemtation
 */
ExternalCopyDate::ExternalCopyDate(const Local<Value>& value) : value(Local<Date>::Cast(value)->ValueOf()) {}

Local<Value> ExternalCopyDate::CopyInto() const {
	return Date::New(Isolate::GetCurrent(), value);
}

size_t ExternalCopyDate::Size() const {
	return sizeof(double);
}

/**
 * ExternalCopyArrayBuffer implemtation
 */
ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const void* data, size_t length) : value(malloc(length), std::free), length(length) {
	std::memcpy(value.get(), data, length);
}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const Local<ArrayBufferView>& handle) : value(malloc(handle->ByteLength()), std::free), length(handle->ByteLength()) {
	handle->CopyContents(value.get(), length);
}

Local<Value> ExternalCopyArrayBuffer::CopyInto() const {
	Local<ArrayBuffer> array_buffer(ArrayBuffer::New(Isolate::GetCurrent(), length));
	std::memcpy(array_buffer->GetContents().Data(), value.get(), length);
	return array_buffer;
}

size_t ExternalCopyArrayBuffer::Size() const {
	return length;
}

const void* ExternalCopyArrayBuffer::Data() const {
	return value.get();
}

size_t ExternalCopyArrayBuffer::Length() const {
	return length;
}

/**
 * ExternalCopyArrayBufferView implemtation
 */
ExternalCopyArrayBufferView::ExternalCopyArrayBufferView(const Local<ArrayBufferView>& handle, ViewType type) : buffer(handle), type(type) {}

Local<Value> ExternalCopyArrayBufferView::CopyInto() const {
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

size_t ExternalCopyArrayBufferView::Size() const {
	return buffer.Size();
}

} // namespace ivm
