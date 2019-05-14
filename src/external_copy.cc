#include "external_copy.h"

#include "isolate_handle.h"
#include "isolate/allocator.h"
#include "isolate/environment.h"
#include "isolate/functor_runners.h"
#include "isolate/util.h"
#include "isolate/v8_version.h"

#include <algorithm>
#include <cstring>

using namespace v8;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * ExternalCopy implementation
 */
// Statics
std::atomic<size_t> ExternalCopy::total_allocated_size { 0 };

size_t ExternalCopy::TotalExternalSize() {
	return total_allocated_size;
}

unique_ptr<ExternalCopy> ExternalCopy::Copy(const Local<Value>& value, bool transfer_out, const handle_vector_t& transfer_list) {
	unique_ptr<ExternalCopy> copy = CopyIfPrimitive(value);
	if (copy) {
		return copy;
	} else if (value->IsDate()) {
		return make_unique<ExternalCopyDate>(value);
	} else if (value->IsArrayBuffer()) {
		Local<ArrayBuffer> array_buffer = Local<ArrayBuffer>::Cast(value);
		if (!transfer_out) {
			transfer_out = std::find(transfer_list.begin(), transfer_list.end(), array_buffer) != transfer_list.end();
		}
		if (transfer_out) {
			return ExternalCopyArrayBuffer::Transfer(array_buffer);
		} else {
			return make_unique<ExternalCopyArrayBuffer>(array_buffer);
		}
	} else if (value->IsSharedArrayBuffer()) {
		return make_unique<ExternalCopySharedArrayBuffer>(value.As<SharedArrayBuffer>());
	} else if (value->IsArrayBufferView()) {
		Local<ArrayBufferView> view = value.As<ArrayBufferView>();
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

		// Sometimes TypedArrays don't actually have a real buffer allocated for them. The call to
		// `Buffer()` below will force v8 to attempt to create a buffer if it doesn't exist, and if
		// there is an allocation failure it will crash the process.
		if (!view->HasBuffer()) {
			auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
			if (allocator != nullptr && !allocator->Check(view->ByteLength())) {
				throw js_range_error("Array buffer allocation failed");
			}
		}

		// `Buffer()` returns a Local<ArrayBuffer> but it may be a Local<SharedArrayBuffer>
		Local<Object> tmp = view->Buffer();
		if (tmp->IsArrayBuffer()) {
			Local<ArrayBuffer> array_buffer = tmp.As<ArrayBuffer>();
			if (!transfer_out) {
				transfer_out = std::find(transfer_list.begin(), transfer_list.end(), array_buffer) != transfer_list.end();
			}
			// Grab byte_offset and byte_length before the transfer because "neutering" the array buffer will null these out
			size_t byte_offset = view->ByteOffset();
			size_t byte_length = view->ByteLength();
			std::unique_ptr<ExternalCopyArrayBuffer> external_buffer;
			if (transfer_out) {
				external_buffer = ExternalCopyArrayBuffer::Transfer(array_buffer);
			} else {
				external_buffer = make_unique<ExternalCopyArrayBuffer>(array_buffer);
			}
			return make_unique<ExternalCopyArrayBufferView>(std::move(external_buffer), type, byte_offset, byte_length);
		} else {
			assert(tmp->IsSharedArrayBuffer());
			Local<SharedArrayBuffer> array_buffer = tmp.As<SharedArrayBuffer>();
			return make_unique<ExternalCopyArrayBufferView>(make_unique<ExternalCopySharedArrayBuffer>(array_buffer), type, view->ByteOffset(), view->ByteLength());
		}
	} else if (value->IsObject()) {
		// Initialize serializer and transferred buffer vectors
		Isolate* isolate = Isolate::GetCurrent();
		transferable_vector_t references;
		array_buffer_vector_t transferred_buffers;
		transferred_buffers.reserve(transfer_list.size());
		shared_buffer_vector_t shared_buffers;
		ExternalCopySerializerDelegate delegate(references, shared_buffers);
		ValueSerializer serializer(isolate, &delegate);
		// Mark array buffers as transferred (transfer must happen *after* serialization)
		for (size_t ii = 0; ii < transfer_list.size(); ++ii) {
			Local<Value> handle = transfer_list[ii];
			if (handle->IsArrayBuffer()) {
				serializer.TransferArrayBuffer(ii, handle.As<ArrayBuffer>());
			} else {
				throw js_type_error("Non-ArrayBuffer passed in `transferList`");
			}
		}
		// Serialize object and perform array buffer transfer
		delegate.serializer = &serializer;
		serializer.WriteHeader();
		Unmaybe(serializer.WriteValue(isolate->GetCurrentContext(), value));
		for (auto& handle : transfer_list) {
			transferred_buffers.emplace_back(ExternalCopyArrayBuffer::Transfer(handle.As<ArrayBuffer>()));
		}
		// Create ExternalCopy instance
		return make_unique<ExternalCopySerialized>(
			serializer.Release(),
			std::move(references),
			std::move(transferred_buffers),
			std::move(shared_buffers)
		);
	} else {
		// ???
		assert(false);
		throw std::logic_error("Exotic value passed to ExternalCopy");
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
		return make_unique<ExternalCopyString>(value.As<String>());
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
		Isolate* isolate = Isolate::GetCurrent();
		Local<Object> object(Local<Object>::Cast(value));
		std::string name = *String::Utf8Value{isolate, object->GetConstructorName()};
		auto error_type = (ExternalCopyError::ErrorType)0;
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
			Local<Context> context = isolate->GetCurrentContext();
			TryCatch try_catch(isolate);
			unique_ptr<ExternalCopyString> message_copy;
			try {
				Local<Value> message(Unmaybe(object->Get(context, v8_symbol("message"))));
				if (message->IsString()) {
					message_copy = make_unique<ExternalCopyString>(message.As<String>());
				} else {
					message_copy = make_unique<ExternalCopyString>("");
				}
			} catch (const js_runtime_error& cc_err) {
				try_catch.Reset();
				message_copy = make_unique<ExternalCopyString>("");
			}

			// Get `stack`
			unique_ptr<ExternalCopyString> stack_copy;
			try {
				Local<Value> stack(Unmaybe(object->Get(context, v8_symbol("stack"))));
				if (stack->IsString()) {
					stack_copy = make_unique<ExternalCopyString>(stack.As<String>());
				}
			} catch (const js_runtime_error& cc_err) {
				try_catch.Reset();
			}
			return make_unique<ExternalCopyError>(error_type, std::move(message_copy), std::move(stack_copy));
		}
	}
	return CopyIfPrimitive(value);
}

// Instance methods
ExternalCopy::ExternalCopy() = default;

ExternalCopy::ExternalCopy(size_t size) : size{size}, original_size{size} {
	total_allocated_size += size;
}

ExternalCopy::ExternalCopy(ExternalCopy&& that) : size{that.size}, original_size{that.size} {
	that.size = 0;
}

ExternalCopy::~ExternalCopy() {
	total_allocated_size -= size;
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

size_t ExternalCopy::OriginalSize() const {
	return original_size;
}

void ExternalCopy::UpdateSize(size_t size) {
	total_allocated_size -= static_cast<ptrdiff_t>(this->size) - static_cast<ptrdiff_t>(size);
	this->size = size;
}

/**
 * ExternalCopyString implementation
 */

// External two byte
ExternalCopyString::ExternalString::ExternalString(shared_ptr<V> value) : value(std::move(value)) {
	IsolateEnvironment::GetCurrent()->extra_allocated_memory += this->value->size();
}

ExternalCopyString::ExternalString::~ExternalString() {
	IsolateEnvironment::GetCurrent()->extra_allocated_memory -= this->value->size();
}

const uint16_t* ExternalCopyString::ExternalString::data() const {
	return reinterpret_cast<uint16_t*>(value->data());
}

size_t ExternalCopyString::ExternalString::length() const {
	return value->size() >> 1;
}

// External one byte
ExternalCopyString::ExternalStringOneByte::ExternalStringOneByte(shared_ptr<V> value) : value(std::move(value)) {
	IsolateEnvironment::GetCurrent()->extra_allocated_memory += this->value->size();
}

ExternalCopyString::ExternalStringOneByte::~ExternalStringOneByte() {
	IsolateEnvironment::GetCurrent()->extra_allocated_memory -= this->value->size();
}

const char* ExternalCopyString::ExternalStringOneByte::data() const {
	return value->data();
}

size_t ExternalCopyString::ExternalStringOneByte::length() const {
	return value->size();
}

// External copy
ExternalCopyString::ExternalCopyString(Local<String> string) : ExternalCopy((string->Length() << (string->IsOneByte() ? 0 : 1)) + sizeof(ExternalCopyString)) {
	if (string->IsOneByte()) {
		one_byte = true;
		value = std::make_shared<V>(string->Length());
#if V8_AT_LEAST(6, 9, 408)
		string->WriteOneByte(Isolate::GetCurrent(),
#else
		string->WriteOneByte(
#endif
			reinterpret_cast<uint8_t*>(value->data()), 0, -1, String::WriteOptions::NO_NULL_TERMINATION
		);
	} else {
		one_byte = false;
		value = std::make_shared<V>(string->Length() << 1);
#if V8_AT_LEAST(6, 9, 408)
		string->Write(Isolate::GetCurrent(),
#else
		string->Write(
#endif
			reinterpret_cast<uint16_t*>(value->data()), 0, -1, String::WriteOptions::NO_NULL_TERMINATION
		);
	}
}

ExternalCopyString::ExternalCopyString(const char* message) : one_byte(true), value(std::make_shared<V>(message, message + strlen(message))) {}

ExternalCopyString::ExternalCopyString(const std::string& message) : one_byte(true), value(std::make_shared<V>(message.begin(), message.end())) {}

Local<Value> ExternalCopyString::CopyInto(bool /*transfer_in*/) {
	if (value->size() < 1024) {
		// Strings under 1kb will be internal v8 strings. I didn't experiment with this at all, but it
		// seems self-evident that there's some byte length under which it doesn't make sense to create
		// an external string so I picked 1kb.
		if (one_byte) {
			return Unmaybe(String::NewFromOneByte(Isolate::GetCurrent(), reinterpret_cast<uint8_t*>(value->data()), NewStringType::kNormal, value->size()));
		} else {
			return Unmaybe(String::NewFromTwoByte(Isolate::GetCurrent(), reinterpret_cast<uint16_t*>(value->data()), NewStringType::kNormal, value->size() >> 1));
		}
	} else {
		// External strings can save memory and/or copies
		if (one_byte) {
			return Unmaybe(String::NewExternalOneByte(Isolate::GetCurrent(), new ExternalStringOneByte(value)));
		} else {
			return Unmaybe(String::NewExternalTwoByte(Isolate::GetCurrent(), new ExternalString(value)));
		}
	}
}

uint32_t ExternalCopyString::WorstCaseHeapSize() const {
	return value->size();
}

/**
 * ExternalCopySerialized implementation
 */
ExternalCopySerialized::ExternalCopySerialized(
	std::pair<uint8_t*, size_t> val,
	transferable_vector_t references,
	array_buffer_vector_t array_buffers,
	shared_buffer_vector_t shared_buffers
) :
	ExternalCopy(val.second + sizeof(ExternalCopySerialized)),
	buffer(val.first, std::free),
	size(val.second),
	references(std::move(references)),
	array_buffers(std::move(array_buffers)),
	shared_buffers(std::move(shared_buffers)) {}

Local<Value> ExternalCopySerialized::CopyInto(bool transfer_in) {
	// Initialize deserializer
	Isolate* isolate = Isolate::GetCurrent();
	Local<Context> context = isolate->GetCurrentContext();
	auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
	int failures = allocator == nullptr ? 0 : allocator->GetFailureCount();
	ExternalCopyDeserializerDelegate delegate(references);
	ValueDeserializer deserializer(isolate, buffer.get(), size, &delegate);
	delegate.deserializer = &deserializer;
	// Transfer array buffers into isolate
	for (size_t ii = 0; ii < array_buffers.size(); ++ii) {
		deserializer.TransferArrayBuffer(ii, array_buffers[ii]->CopyIntoCheckHeap(transfer_in).As<ArrayBuffer>());
	}
	for (size_t ii = 0; ii < shared_buffers.size(); ++ii) {
		deserializer.TransferSharedArrayBuffer(ii, shared_buffers[ii]->CopyIntoCheckHeap(false).As<SharedArrayBuffer>());
	}
	// Deserialize object
	Unmaybe(deserializer.ReadHeader(context));
	Local<Value> value;
	if (deserializer.ReadValue(context).ToLocal(&value)) {
		return value;
	} else {
		// ValueDeserializer throws an unhelpful message when it fails to allocate an ArrayBuffer, so
		// detect that case here and throw an appropriate message.
		if (allocator != nullptr && allocator->GetFailureCount() != failures) {
			throw js_range_error("Array buffer allocation failed");
		} else {
			throw js_runtime_error();
		}
	}
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

ExternalCopyError::ExternalCopyError(ErrorType error_type, const char* message, std::string stack) :
	ExternalCopy(sizeof(ExternalCopyError)),
	error_type(error_type),
	message(make_unique<ExternalCopyString>(message)),
	stack(stack.empty() ? unique_ptr<ExternalCopyString>() : make_unique<ExternalCopyString>(std::move(stack))) {}

Local<Value> ExternalCopyError::CopyInto(bool /*transfer_in*/) {

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
		Unmaybe(Local<Object>::Cast(handle)->Set(Isolate::GetCurrent()->GetCurrentContext(), v8_symbol("stack"), stack));
	}
	return handle;
}

uint32_t ExternalCopyError::WorstCaseHeapSize() const {
	return 128 + (message ? message->WorstCaseHeapSize() : 0) + (stack ? stack->WorstCaseHeapSize() : 0);
}

/**
 * ExternalCopyNull and ExternalCopyUndefined implementations
 */
Local<Value> ExternalCopyNull::CopyInto(bool /*transfer_in*/) {
	return Null(Isolate::GetCurrent());
}

uint32_t ExternalCopyNull::WorstCaseHeapSize() const {
	return 16;
}

Local<Value> ExternalCopyUndefined::CopyInto(bool /*transfer_in*/) {
	return Undefined(Isolate::GetCurrent());
}

uint32_t ExternalCopyUndefined::WorstCaseHeapSize() const {
	return 16;
}

/**
 * ExternalCopyDate implementation
 */
ExternalCopyDate::ExternalCopyDate(const Local<Value>& value) : ExternalCopy(sizeof(ExternalCopyDate)), value(Local<Date>::Cast(value)->ValueOf()) {}

Local<Value> ExternalCopyDate::CopyInto(bool /*transfer_in*/) {
	return Unmaybe(Date::New(Isolate::GetCurrent()->GetCurrentContext(), value));
}

uint32_t ExternalCopyDate::WorstCaseHeapSize() const {
	// Seems pretty excessive, but that's what I observed
	return 140;
}

/**
 * ExternalCopyBytes implementation
 */
ExternalCopyBytes::ExternalCopyBytes(size_t size) : ExternalCopy(size) {}

ExternalCopyBytes::Holder::Holder(const Local<Object>& buffer, shared_ptr<void> cc_ptr, size_t size) :
	v8_ptr(Isolate::GetCurrent(), buffer), cc_ptr(std::move(cc_ptr)), size(size)
{
	v8_ptr.SetWeak(reinterpret_cast<void*>(this), &WeakCallbackV8, WeakCallbackType::kParameter);
	IsolateEnvironment::GetCurrent()->AddWeakCallback(&this->v8_ptr, WeakCallback, this);
	buffer->SetAlignedPointerInInternalField(0, this);
	IsolateEnvironment::GetCurrent()->extra_allocated_memory += size;
}

ExternalCopyBytes::Holder::~Holder() {
	v8_ptr.Reset();
	if (cc_ptr) {
		IsolateEnvironment::GetCurrent()->extra_allocated_memory -= size;
	}
}

void ExternalCopyBytes::Holder::WeakCallbackV8(const WeakCallbackInfo<void>& info) {
	WeakCallback(info.GetParameter());
}

void ExternalCopyBytes::Holder::WeakCallback(void* param) {
	auto that = reinterpret_cast<Holder*>(param);
	IsolateEnvironment::GetCurrent()->RemoveWeakCallback(&that->v8_ptr);
	delete that;
}

/**
 * ExternalCopyArrayBuffer implementation
 */
ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const void* data, size_t length) :
	ExternalCopyBytes(length + sizeof(ExternalCopyArrayBuffer)),
	value(shared_ptr<void>(malloc(length), std::free)),
	length(length)
{
	std::memcpy(value.get(), data, length);
}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(shared_ptr<void> ptr, size_t length) :
	ExternalCopyBytes(length + sizeof(ExternalCopyArrayBuffer)),
	value(std::move(ptr)),
	length(length) {}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const Local<ArrayBuffer>& handle) :
	ExternalCopyBytes(handle->ByteLength() + sizeof(ExternalCopyArrayBuffer)),
	value(malloc(handle->ByteLength()), std::free),
	length(handle->ByteLength())
{
	std::memcpy(value.get(), handle->GetContents().Data(), length);
}

unique_ptr<ExternalCopyArrayBuffer> ExternalCopyArrayBuffer::Transfer(const Local<ArrayBuffer>& handle) {
	size_t length = handle->ByteLength();
	if (length == 0) {
		throw js_generic_error("Array buffer is invalid");
	}
	if (handle->IsExternal()) {
		// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
		auto ptr = reinterpret_cast<Holder*>(handle->GetAlignedPointerFromInternalField(0));
		if (!handle->IsNeuterable() || ptr == nullptr || ptr->magic != Holder::kMagic) { // dangerous
			throw js_generic_error("Array buffer cannot be externalized");
		}
		handle->Neuter();
		IsolateEnvironment::GetCurrent()->extra_allocated_memory -= length;
		// No race conditions here because only one thread can access `Holder`
		return std::make_unique<ExternalCopyArrayBuffer>(std::move(ptr->cc_ptr), length);
	}
	// In this case the buffer is internal and can be released easily
	ArrayBuffer::Contents contents = handle->Externalize();
	auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
	if (allocator != nullptr) {
		allocator->AdjustAllocatedSize(-static_cast<ptrdiff_t>(length));
	}
	assert(handle->IsNeuterable());
	auto data_ptr = shared_ptr<void>(contents.Data(), std::free);
	handle->Neuter();
	return std::make_unique<ExternalCopyArrayBuffer>(std::move(data_ptr), length);
}

Local<Value> ExternalCopyArrayBuffer::CopyInto(bool transfer_in) {
	if (transfer_in) {
		auto tmp = std::atomic_exchange(&value, shared_ptr<void>());
		if (tmp == nullptr) {
			throw js_generic_error("Array buffer is invalid");
		}
		if (tmp.use_count() != 1) {
			std::atomic_store(&value, std::move(tmp));
			throw js_generic_error("Array buffer is in use and may not be transferred");
		}
		UpdateSize(0);
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), tmp.get(), length);
		new Holder(array_buffer, std::move(tmp), length);
		return array_buffer;
	} else {
		auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
		if (allocator != nullptr && !allocator->Check(length)) {
			// ArrayBuffer::New will crash the process if there is an allocation failure, so we check
			// here.
			throw js_range_error("Array buffer allocation failed");
		}
		auto ptr = GetSharedPointer();
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), length);
		std::memcpy(array_buffer->GetContents().Data(), ptr.get(), length);
		return array_buffer;
	}
}

uint32_t ExternalCopyArrayBuffer::WorstCaseHeapSize() const {
	return length;
}

shared_ptr<void> ExternalCopyArrayBuffer::GetSharedPointer() const {
	auto ptr = std::atomic_load(&value);
	if (!ptr) {
		throw js_generic_error("Array buffer is invalid");
	}
	return ptr;
}

const void* ExternalCopyArrayBuffer::Data() const {
	return value.get();
}

size_t ExternalCopyArrayBuffer::Length() const {
	return length;
}

/**
 * ExternalCopySharedArrayBuffer implementation
 */
ExternalCopySharedArrayBuffer::ExternalCopySharedArrayBuffer(const v8::Local<v8::SharedArrayBuffer>& handle) :
	ExternalCopyBytes(handle->ByteLength() + sizeof(ExternalCopySharedArrayBuffer)),
	length(handle->ByteLength())
{
	// Similar to ArrayBuffer::Transfer but different enough to make it not worth abstracting out..
	size_t length = handle->ByteLength();
	if (length == 0) {
		throw js_generic_error("Array buffer is invalid");
	}
	if (handle->IsExternal()) {
		// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
		auto ptr = reinterpret_cast<Holder*>(handle->GetAlignedPointerFromInternalField(0));
		if (ptr == nullptr || ptr->magic != Holder::kMagic) { // dangerous pointer dereference
			throw js_generic_error("Array buffer cannot be externalized");
		}
		// No race conditions here because only one thread can access `Holder`
		value = ptr->cc_ptr;
		return;
	}
	// In this case the buffer is internal and should be externalized
	SharedArrayBuffer::Contents contents = handle->Externalize();
	value = shared_ptr<void>(contents.Data(), std::free);
	new Holder(handle, value, length);
	// Adjust allocator memory down, and `Holder` will adjust memory back up
	auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
	if (allocator != nullptr) {
		allocator->AdjustAllocatedSize(-static_cast<ptrdiff_t>(length));
	}
}

Local<Value> ExternalCopySharedArrayBuffer::CopyInto(bool /*transfer_in*/) {
	auto ptr = GetSharedPointer();
	Local<SharedArrayBuffer> array_buffer = SharedArrayBuffer::New(Isolate::GetCurrent(), ptr.get(), length);
	new Holder(array_buffer, std::move(ptr), length);
	return array_buffer;
}

uint32_t ExternalCopySharedArrayBuffer::WorstCaseHeapSize() const {
	return length;
}

shared_ptr<void> ExternalCopySharedArrayBuffer::GetSharedPointer() const {
	auto ptr = std::atomic_load(&value);
	if (!ptr) {
		throw js_generic_error("Array buffer is invalid");
	}
	return ptr;
}

/**
 * ExternalCopyArrayBufferView implementation
 */
ExternalCopyArrayBufferView::ExternalCopyArrayBufferView(
	std::unique_ptr<ExternalCopyBytes> buffer,
	ViewType type, size_t byte_offset, size_t byte_length
) :
	ExternalCopy(sizeof(ExternalCopyArrayBufferView)),
	buffer(std::move(buffer)),
	type(type),
	byte_offset(byte_offset), byte_length(byte_length) {}

template <typename T>
Local<Value> NewTypedArrayView(Local<T> buffer, ExternalCopyArrayBufferView::ViewType type, size_t byte_offset, size_t byte_length) {
	switch (type) {
		case ExternalCopyArrayBufferView::ViewType::Uint8:
			return Uint8Array::New(buffer, byte_offset, byte_length >> 0);
		case ExternalCopyArrayBufferView::ViewType::Uint8Clamped:
			return Uint8ClampedArray::New(buffer, byte_offset, byte_length >> 0);
		case ExternalCopyArrayBufferView::ViewType::Int8:
			return Int8Array::New(buffer, byte_offset, byte_length >> 0);
		case ExternalCopyArrayBufferView::ViewType::Uint16:
			return Uint16Array::New(buffer, byte_offset, byte_length >> 1);
		case ExternalCopyArrayBufferView::ViewType::Int16:
			return Int16Array::New(buffer, byte_offset, byte_length >> 1);
		case ExternalCopyArrayBufferView::ViewType::Uint32:
			return Uint32Array::New(buffer, byte_offset, byte_length >> 2);
		case ExternalCopyArrayBufferView::ViewType::Int32:
			return Int32Array::New(buffer, byte_offset, byte_length >> 2);
		case ExternalCopyArrayBufferView::ViewType::Float32:
			return Float32Array::New(buffer, byte_offset, byte_length >> 2);
		case ExternalCopyArrayBufferView::ViewType::Float64:
			return Float64Array::New(buffer, byte_offset, byte_length >> 3);
		case ExternalCopyArrayBufferView::ViewType::DataView:
			return DataView::New(buffer, byte_offset, byte_length);
		default:
			throw std::exception();
	}
}

Local<Value> ExternalCopyArrayBufferView::CopyInto(bool transfer_in) {
	Local<Value> buffer = this->buffer->CopyInto(transfer_in);
	if (buffer->IsArrayBuffer()) {
		return NewTypedArrayView(buffer.As<ArrayBuffer>(), type, byte_offset, byte_length);
	} else {
		return NewTypedArrayView(buffer.As<SharedArrayBuffer>(), type, byte_offset, byte_length);
	}
}

uint32_t ExternalCopyArrayBufferView::WorstCaseHeapSize() const {
	// This includes the overhead of the ArrayBuffer
	return 208 + buffer->WorstCaseHeapSize();
}

} // namespace ivm
