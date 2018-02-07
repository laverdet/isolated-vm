#include "external_copy.h"

#include "isolate_handle.h"
#include "isolate/environment.h"
#include "isolate/util.h"

#include <cstring>

using namespace v8;
using std::make_unique;
using std::unique_ptr;

namespace ivm {

/**
 * ExternalCopy implementation
 */
unique_ptr<ExternalCopy> ExternalCopy::Copy(const Local<Value>& value, bool transfer_out) {
	unique_ptr<ExternalCopy> copy = CopyIfPrimitive(value);
	if (copy) {
		return copy;
	} else if (value->IsDate()) {
		return make_unique<ExternalCopyDate>(value);
	} else if (value->IsArrayBuffer()) {
		Local<ArrayBuffer> array_buffer = Local<ArrayBuffer>::Cast(value);
		if (transfer_out) {
			return ExternalCopyArrayBuffer::Transfer(array_buffer);
		} else {
			ArrayBuffer::Contents contents(array_buffer->GetContents());
			return make_unique<ExternalCopyArrayBuffer>(contents.Data(), contents.ByteLength());
		}
	} else if (value->IsArrayBufferView()) {
		Local<ArrayBufferView> view(Local<ArrayBufferView>::Cast(value));
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
		if (transfer_out) {
			Local<ArrayBuffer> array_buffer = view->Buffer();
			if (view->ByteOffset() != 0 || view->ByteLength() != array_buffer->ByteLength()) {
				throw js_generic_error("Cannot transfer sliced TypedArray (this.byteOffset != 0 || this.byteLength != this.buffer.byteLength)");
			}
			return make_unique<ExternalCopyArrayBufferView>(ExternalCopyArrayBuffer::Transfer(array_buffer), type);
		} else {
			return make_unique<ExternalCopyArrayBufferView>(view, type);
		}
	} else if (value->IsObject()) {
		Isolate* isolate = Isolate::GetCurrent();
		ValueSerializer serializer(isolate);
		serializer.WriteHeader();
		Unmaybe(serializer.WriteValue(isolate->GetCurrentContext(), value));
		return make_unique<ExternalCopySerialized>(serializer.Release());
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

Local<Value> ExternalCopy::CopyIntoCheckHeap(bool transfer_in) {
	IsolateEnvironment::HeapCheck heap_check(*IsolateEnvironment::GetCurrent(), WorstCaseHeapSize());
	auto value = CopyInto(transfer_in);
	heap_check.Epilogue();
	return value;
}

Local<Value> ExternalCopy::TransferIn() {
	return CopyIntoCheckHeap();
}

/**
 * ExternalCopySerialized implementation
 */
ExternalCopySerialized::ExternalCopySerialized(std::pair<uint8_t*, size_t> val) : buffer(val.first, std::free), size(val.second) {}

Local<Value> ExternalCopySerialized::CopyInto(bool transfer_in) {
	Isolate* isolate = Isolate::GetCurrent();
	Local<Context> context = isolate->GetCurrentContext();
	ValueDeserializer deserializer(isolate, buffer.get(), size);
	Unmaybe(deserializer.ReadHeader(context));
	return Unmaybe(deserializer.ReadValue(context));
}

size_t ExternalCopySerialized::Size() const {
	return size;
}

uint32_t ExternalCopySerialized::WorstCaseHeapSize() const {
	// The worst I could come up with was an array of strings each with length 2.
	return size * 6;
}

/**
 * ExternalCopyError implementation
 */
ExternalCopyError::ExternalCopyError(
	ErrorType error_type,
	unique_ptr<ExternalCopyString> message,
	unique_ptr<ExternalCopyString> stack)
	: error_type(error_type),
	message(std::move(message)),
	stack(std::move(stack)) {}

ExternalCopyError::ExternalCopyError(ErrorType error_type, const std::string& message) : error_type(error_type), message(make_unique<ExternalCopyString>(message)) {}

Local<Value> ExternalCopyError::CopyInto(bool transfer_in) {

	// First make the exception w/ correct + message
	Local<String> message(Local<String>::Cast(this->message->CopyInto(false)));
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
		Local<String> stack(Local<String>::Cast(this->stack->CopyInto(false)));
		Local<Object>::Cast(handle)->Set(v8_symbol("stack"), stack);
	}
	return handle;
}

size_t ExternalCopyError::Size() const {
	return
		(message ? message->Size() : 0) +
		(stack ? stack->Size() : 0);
}

uint32_t ExternalCopyError::WorstCaseHeapSize() const {
	return 128 + (message ? message->WorstCaseHeapSize() : 0) + (stack ? stack->WorstCaseHeapSize() : 0);
}

/**
 * ExternalCopyNull and ExternalCopyUndefined implementations
 */
Local<Value> ExternalCopyNull::CopyInto(bool transfer_in) {
	return Null(Isolate::GetCurrent());
}

size_t ExternalCopyNull::Size() const {
	return 0;
}

uint32_t ExternalCopyNull::WorstCaseHeapSize() const {
	return 16;
}

Local<Value> ExternalCopyUndefined::CopyInto(bool transfer_in) {
	return Undefined(Isolate::GetCurrent());
}

size_t ExternalCopyUndefined::Size() const {
	return 0;
}

uint32_t ExternalCopyUndefined::WorstCaseHeapSize() const {
	return 16;
}

/**
 * ExternalCopyDate implementation
 */
ExternalCopyDate::ExternalCopyDate(const Local<Value>& value) : value(Local<Date>::Cast(value)->ValueOf()) {}

Local<Value> ExternalCopyDate::CopyInto(bool transfer_in) {
	return Date::New(Isolate::GetCurrent(), value);
}

size_t ExternalCopyDate::Size() const {
	return sizeof(double);
}

uint32_t ExternalCopyDate::WorstCaseHeapSize() const {
	// Seems pretty excessive, but that's what I observed
	return 140;
}

/**
 * ExternalCopyArrayBuffer implementation
 */
ExternalCopyArrayBuffer::Holder::Holder(const Local<ArrayBuffer>& buffer, ptr_t cc_ptr) : v8_ptr(Isolate::GetCurrent(), buffer), cc_ptr(std::move(cc_ptr)) {
	v8_ptr.SetWeak(reinterpret_cast<void*>(this), &WeakCallbackV8, WeakCallbackType::kParameter);
	IsolateEnvironment::GetCurrent()->AddWeakCallback(&this->v8_ptr, WeakCallback, this);
	buffer->SetAlignedPointerInInternalField(0, this);
}

void ExternalCopyArrayBuffer::Holder::WeakCallbackV8(const v8::WeakCallbackInfo<void>& info) {
	WeakCallback(info.GetParameter());
}

void ExternalCopyArrayBuffer::Holder::WeakCallback(void* param) {
	Holder* that = reinterpret_cast<Holder*>(param);
	IsolateEnvironment::GetCurrent()->RemoveWeakCallback(&that->v8_ptr);
	delete that;
}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const void* data, size_t length) : value(malloc(length), std::free), length(length) {
	std::memcpy(value.get(), data, length);
}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(ptr_t ptr, size_t length) : value(std::move(ptr)), length(length) {}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const Local<ArrayBufferView>& handle) : value(malloc(handle->ByteLength()), std::free), length(handle->ByteLength()) {
	if (handle->CopyContents(value.get(), length) != length) {
		throw js_generic_error("Failed to copy array contents");
	}
}

unique_ptr<ExternalCopyArrayBuffer> ExternalCopyArrayBuffer::Transfer(const Local<ArrayBuffer>& handle) {
	size_t length = handle->ByteLength();
	if (length == 0) {
		throw js_generic_error("Array buffer is invalid");
	}
	if (handle->IsExternal()) {
		// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
		Holder* ptr = reinterpret_cast<Holder*>(handle->GetAlignedPointerFromInternalField(0));
		if (!handle->IsNeuterable() || ptr == nullptr || ptr->magic != Holder::kMagic) { // dangerous
			throw js_generic_error("Array buffer cannot be externalized");
		}
		handle->Neuter();
		return std::make_unique<ExternalCopyArrayBuffer>(std::move(ptr->cc_ptr), length);
	}
	// In this case the buffer is internal and can be released easily
	ArrayBuffer::Contents contents = handle->Externalize();
	auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
	if (allocator != nullptr) {
		allocator->AdjustAllocatedSize(-length);
	}
	assert(handle->IsNeuterable());
	auto data_ptr = ptr_t(contents.Data(), std::free);
	handle->Neuter();
	return std::make_unique<ExternalCopyArrayBuffer>(std::move(data_ptr), length);
}

Local<Value> ExternalCopyArrayBuffer::CopyInto(bool transfer_in) {
	if (transfer_in) {
		auto tmp = ptr_t(nullptr, std::free);
		std::swap(tmp, value);
		if (!tmp) {
			throw js_generic_error("Array buffer is invalid");
		}
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), tmp.get(), length);
		new Holder(array_buffer, std::move(tmp));
		return array_buffer;
	} else {
		// There is a race condition here if you try to copy and transfer at the same time. Don't do
		// that.
		void* ptr = value.get();
		if (ptr == nullptr) {
			throw js_generic_error("Array buffer is invalid");
		}
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), length);
		std::memcpy(array_buffer->GetContents().Data(), ptr, length);
		return array_buffer;
	}
}

size_t ExternalCopyArrayBuffer::Size() const {
	return length;
}

uint32_t ExternalCopyArrayBuffer::WorstCaseHeapSize() const {
	// ArrayBuffers use system heap, not the flimsy v8 heap
	return 96;
}

const void* ExternalCopyArrayBuffer::Data() const {
	return value.get();
}

size_t ExternalCopyArrayBuffer::Length() const {
	return length;
}

/**
 * ExternalCopyArrayBufferView implementation
 */
ExternalCopyArrayBufferView::ExternalCopyArrayBufferView(const Local<ArrayBufferView>& handle, ViewType type) : buffer(std::make_unique<ExternalCopyArrayBuffer>(handle)), type(type) {}

ExternalCopyArrayBufferView::ExternalCopyArrayBufferView(std::unique_ptr<ExternalCopyArrayBuffer> buffer, ViewType type) : buffer(std::move(buffer)), type(type) {}

Local<Value> ExternalCopyArrayBufferView::CopyInto(bool transfer_in) {
	Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(this->buffer->CopyInto(transfer_in));
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
	return buffer->Size();
}

uint32_t ExternalCopyArrayBufferView::WorstCaseHeapSize() const {
	// This includes the overhead of the ArrayBuffer
	return 208;
}

} // namespace ivm
