#include "external_copy.h"
#include "error.h"
#include "serializer.h"
#include "./string.h"

#include "isolate/allocator.h"
#include "isolate/environment.h"
#include "isolate/functor_runners.h"
#include "isolate/util.h"
#include "isolate/v8_version.h"

#include <algorithm>
#include <cstring>

using namespace v8;

namespace ivm {
namespace {

/**
 * This is used for Number (several C++ types), or Boolean.
 */
template <class Type>
struct ExternalCopyTemplateCtor {
	template <class Native>
	static auto New(Isolate* isolate, Native value) -> Local<Value> {
		return Type::New(isolate, value);
	}
};

template <>
struct ExternalCopyTemplateCtor<Uint32> {
	static auto New(Isolate* isolate, uint32_t value) -> Local<Value> {
		return Uint32::NewFromUnsigned(isolate, value);
	}
};

template <class Type, class Native>
class ExternalCopyTemplate : public ExternalCopy {
	public:
		explicit ExternalCopyTemplate(Local<Value> value) :
			ExternalCopy{sizeof(ExternalCopyTemplate)},
			value{value.As<Type>()->Value()} {}

		auto CopyInto(bool /*transfer_in*/ = false) -> Local<Value> final {
			return ExternalCopyTemplateCtor<Type>::New(Isolate::GetCurrent(), value);
		}

	private:
		const Native value;
};

/**
 * BigInt data
 */
#if V8_AT_LEAST(6, 9, 258)
// Added in 477df066dbb
struct ExternalCopyBigInt : public ExternalCopy {
	public:
		explicit ExternalCopyBigInt(Local<BigInt> value) {
			int word_count = value->WordCount();
			words.resize(word_count);
			value->ToWordsArray(&sign_bit, &word_count, words.data());
		}

		auto CopyInto(bool /*transfer_in*/ = false) -> Local<Value> final {
			return Unmaybe(BigInt::NewFromWords(
				Isolate::GetCurrent()->GetCurrentContext(), sign_bit, words.size(), words.data()));
		}

	private:
		int sign_bit = 0;
		std::vector<uint64_t> words;
};
#endif

/**
 * null and undefined
 */
class ExternalCopyNull : public ExternalCopy {
	public:
		auto CopyInto(bool /*transfer_in*/ = false) -> Local<Value> final {
			return Null(Isolate::GetCurrent());
		}
};

class ExternalCopyUndefined : public ExternalCopy {
	public:
		auto CopyInto(bool /*transfer_in*/ = false) -> v8::Local<v8::Value> final {
			return Undefined(Isolate::GetCurrent());
		}
};

// Global size counter
std::atomic<size_t> total_allocated_size {0};

} // anonymous namespace

/**
 * ExternalCopy implementation
 */
ExternalCopy::ExternalCopy(int size) : size{size} {
	total_allocated_size += size;
}

ExternalCopy::ExternalCopy(ExternalCopy&& that) noexcept : size{std::exchange(that.size, 0)} {}

ExternalCopy::~ExternalCopy() {
	total_allocated_size -= size;
}

auto ExternalCopy::operator= (ExternalCopy&& that) noexcept -> ExternalCopy& {
	size = std::exchange(that.size, 0);
	return *this;
}

std::unique_ptr<ExternalCopy> ExternalCopy::Copy(
		Local<Value> value, bool transfer_out, ArrayRange transfer_list) {
	std::unique_ptr<ExternalCopy> copy = CopyIfPrimitive(value);
	if (copy) {
		return copy;
	} else if (value->IsArrayBuffer()) {
		Local<ArrayBuffer> array_buffer = Local<ArrayBuffer>::Cast(value);
		if (!transfer_out) {
			transfer_out = std::find(transfer_list.begin(), transfer_list.end(), array_buffer) != transfer_list.end();
		}
		if (transfer_out) {
			return ExternalCopyArrayBuffer::Transfer(array_buffer);
		} else {
			return std::make_unique<ExternalCopyArrayBuffer>(array_buffer);
		}
	} else if (value->IsSharedArrayBuffer()) {
		return std::make_unique<ExternalCopySharedArrayBuffer>(value.As<SharedArrayBuffer>());
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
		} else if (view->IsBigInt64Array()) {
			type = ViewType::BigInt64Array;
		} else if (view->IsBigUint64Array()) {
			type = ViewType::BigUint64Array;
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
				throw RuntimeRangeError("Array buffer allocation failed");
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
				external_buffer = std::make_unique<ExternalCopyArrayBuffer>(array_buffer);
			}
			return std::make_unique<ExternalCopyArrayBufferView>(std::move(external_buffer), type, byte_offset, byte_length);
		} else {
			assert(tmp->IsSharedArrayBuffer());
			Local<SharedArrayBuffer> array_buffer = tmp.As<SharedArrayBuffer>();
			return std::make_unique<ExternalCopyArrayBufferView>(std::make_unique<ExternalCopySharedArrayBuffer>(array_buffer), type, view->ByteOffset(), view->ByteLength());
		}
	} else if (value->IsObject()) {
		return std::make_unique<ExternalCopySerialized>(value.As<Object>(), transfer_list);
	} else {
		throw RuntimeTypeError("Unsupported type");
	}
}

namespace {
	auto CopyIfPrimitiveImpl(Local<Value> value) -> std::unique_ptr<ExternalCopy> {
		if (value->IsString()) {
			return std::make_unique<ExternalCopyString>(value.As<String>());
		} else if (value->IsNumber()) {
			if (value->IsUint32()) {
				return std::make_unique<ExternalCopyTemplate<Uint32, uint32_t>>(value);
			} else if (value->IsInt32()) {
				return std::make_unique<ExternalCopyTemplate<Int32, int32_t>>(value);
			} else {
				// This handles Infinity, -Infinity, NaN
				return std::make_unique<ExternalCopyTemplate<Number, double>>(value);
			}
#if V8_AT_LEAST(6, 9, 258)
		} else if (value->IsBigInt()) {
			return std::make_unique<ExternalCopyBigInt>(value.As<BigInt>());
#endif
		} else if (value->IsBoolean()) {
			return std::make_unique<ExternalCopyTemplate<Boolean, bool>>(value);
		} else if (value->IsNull()) {
			return std::make_unique<ExternalCopyNull>();
		} else if (value->IsUndefined()) {
			return std::make_unique<ExternalCopyUndefined>();
		}
		return {};
	}
}

auto ExternalCopy::CopyIfPrimitive(Local<Value> value) -> std::unique_ptr<ExternalCopy> {
	if (!value->IsObject()) {
		return CopyIfPrimitiveImpl(value);
	}
	return {};
}

auto ExternalCopy::CopyThrownValue(Local<Value> value) -> std::unique_ptr<ExternalCopy> {
	if (value->IsObject()) {

		// Detect which subclass of Error was thrown (no better way to do this??)
		Isolate* isolate = Isolate::GetCurrent();
		Local<Object> object = Local<Object>::Cast(value);
		std::string name = *String::Utf8Value{isolate, object->GetConstructorName()};
		auto error_type = ExternalCopyError::ErrorType::CustomError;
		if (name == "Error") {
			error_type = ExternalCopyError::ErrorType::Error;
		} else if (name == "RangeError") {
			error_type = ExternalCopyError::ErrorType::RangeError;
		} else if (name == "ReferenceError") {
			error_type = ExternalCopyError::ErrorType::ReferenceError;
		} else if (name == "SyntaxError") {
			error_type = ExternalCopyError::ErrorType::SyntaxError;
		} else if (name == "TypeError") {
			error_type = ExternalCopyError::ErrorType::TypeError;
		}

		// Get error properties
		Local<Context> context = isolate->GetCurrentContext();
		TryCatch try_catch{isolate};
		auto get_property = [&](Local<Object> object, const char* key) {
			try {
				Local<Value> value = Unmaybe(object->Get(context, v8_string(key)));
				if (value->IsString()) {
					return ExternalCopyString{value.As<String>()};
				}
			} catch (const RuntimeError& cc_err) {
				try_catch.Reset();
			}
			return ExternalCopyString{};
		};
		ExternalCopyString message_copy = get_property(object, "message");
		ExternalCopyString stack_copy = get_property(object, "stack");

		// Return external error copy if this looked like an error
		if (error_type != ExternalCopyError::ErrorType::CustomError || message_copy || stack_copy) {
			ExternalCopyString name_copy;
			if (error_type == ExternalCopyError::ErrorType::CustomError) {
				name_copy = get_property(object, "name");
			}
			return std::make_unique<ExternalCopyError>(error_type, std::move(name_copy), std::move(message_copy), std::move(stack_copy));
		}
	}
	auto primitive_value = CopyIfPrimitiveImpl(value);
	if (primitive_value) {
		return primitive_value;
	}
	return std::make_unique<ExternalCopyError>(
		ExternalCopyError::ErrorType::Error,
		"An object was thrown from supplied code within isolated-vm, but that object was not an instance of `Error`."
	);
}

auto ExternalCopy::CopyIntoCheckHeap(bool transfer_in) -> Local<Value> {
	IsolateEnvironment::HeapCheck heap_check{*IsolateEnvironment::GetCurrent()};
	auto value = CopyInto(transfer_in);
	heap_check.Epilogue();
	return value;
}

auto ExternalCopy::TotalExternalSize() -> int {
	return total_allocated_size;
}

void ExternalCopy::UpdateSize(int size) {
	total_allocated_size -= this->size - size;
	this->size = size;
}

/**
 * ExternalCopyError implementation
 */
ExternalCopyError::ExternalCopyError(
	ErrorType error_type,
	ExternalCopyString name,
	ExternalCopyString message,
	ExternalCopyString stack
) :
	error_type{error_type},
	name{std::move(name)},
	message{std::move(message)},
	stack{std::move(stack)} {}

ExternalCopyError::ExternalCopyError(ErrorType error_type, const char* message, const std::string& stack) :
	ExternalCopy{sizeof(ExternalCopyError)},
	error_type{error_type},
	message{ExternalCopyString{message}},
	stack{stack.empty() ? ExternalCopyString{} : ExternalCopyString{stack}} {}

Local<Value> ExternalCopyError::CopyInto(bool /*transfer_in*/) {

	// First make the exception w/ correct + message
	Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
	Local<String> message = Local<String>::Cast(this->message.CopyInto(false));
	Local<Value> handle;
	switch (error_type) {
		default:
			handle = Exception::Error(message);
			if (name) {
				Unmaybe(handle.As<Object>()->DefineOwnProperty(context, StringTable::Get().name, name.CopyInto(), PropertyAttribute::DontEnum));
			}
			break;
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
	}

	// Now add stack information
	if (this->stack) {
		Local<String> stack = Local<String>::Cast(this->stack.CopyInto(false));
		Unmaybe(Local<Object>::Cast(handle)->Set(context, StringTable::Get().stack, stack));
	}
	return handle;
}

/**
 * ExternalCopyBytes implementation
 */
ExternalCopyBytes::ExternalCopyBytes(size_t size, std::shared_ptr<void> value, size_t length) :
		ExternalCopy{static_cast<int>(size)}, value{std::move(value)}, length{length} {}

std::shared_ptr<void> ExternalCopyBytes::Acquire() const {
	std::lock_guard<std::mutex> lock{mutex};
	auto ret = value;
	if (!ret) {
		throw RuntimeGenericError("Array buffer is invalid");
	}
	return ret;
}

std::shared_ptr<void> ExternalCopyBytes::Release() {
	std::lock_guard<std::mutex> lock{mutex};
	return std::move(value);
}

void ExternalCopyBytes::Replace(std::shared_ptr<void> value) {
	std::lock_guard<std::mutex> lock{mutex};
	this->value = std::move(value);
}

ExternalCopyBytes::Holder::Holder(const Local<Object>& buffer, std::shared_ptr<void> cc_ptr, size_t size) :
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
		ExternalCopyBytes{length + sizeof(ExternalCopyArrayBuffer), {malloc(length), std::free}, length} {
	std::memcpy(Acquire().get(), data, length);
}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(std::shared_ptr<void> ptr, size_t length) :
		ExternalCopyBytes{length + sizeof(ExternalCopyArrayBuffer), std::move(ptr), length} {}

ExternalCopyArrayBuffer::ExternalCopyArrayBuffer(const Local<ArrayBuffer>& handle) :
		ExternalCopyBytes{
			handle->ByteLength() + sizeof(ExternalCopyArrayBuffer),
			{malloc(handle->ByteLength()), std::free},
			handle->ByteLength()} {
	std::memcpy(Acquire().get(), handle->GetContents().Data(), Length());
}

std::unique_ptr<ExternalCopyArrayBuffer> ExternalCopyArrayBuffer::Transfer(const Local<ArrayBuffer>& handle) {
	auto Detach = [](Local<ArrayBuffer> handle) {
#if V8_AT_LEAST(7, 3, 89)
		handle->Detach();
#else
		handle->Neuter();
#endif
	};
	auto IsDetachable = [](Local<ArrayBuffer> handle) {
#if V8_AT_LEAST(7, 3, 89)
		return handle->IsDetachable();
#else
		return handle->IsNeuterable();
#endif
	};

	size_t length = handle->ByteLength();
	if (length == 0) {
		throw RuntimeGenericError("Array buffer is invalid");
	}
	if (handle->IsExternal()) {
		// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
		auto ptr = reinterpret_cast<Holder*>(handle->GetAlignedPointerFromInternalField(0));
		if (!IsDetachable(handle) || ptr == nullptr || ptr->magic != Holder::kMagic) { // dangerous
			throw RuntimeGenericError("Array buffer cannot be externalized");
		}
		Detach(handle);
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
	assert(IsDetachable(handle));
	auto data_ptr = std::shared_ptr<void>(contents.Data(), std::free);
	Detach(handle);
	return std::make_unique<ExternalCopyArrayBuffer>(std::move(data_ptr), length);
}

Local<Value> ExternalCopyArrayBuffer::CopyInto(bool transfer_in) {
	if (transfer_in) {
		auto tmp = Release();
		if (!tmp) {
			throw RuntimeGenericError("Array buffer is invalid");
		}
		if (tmp.use_count() != 1) {
			Replace(std::move(tmp));
			throw RuntimeGenericError("Array buffer is in use and may not be transferred");
		}
		UpdateSize(0);
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), tmp.get(), Length());
		new Holder{array_buffer, std::move(tmp), Length()};
		return array_buffer;
	} else {
		auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
		if (allocator != nullptr && !allocator->Check(Length())) {
			// ArrayBuffer::New will crash the process if there is an allocation failure, so we check
			// here.
			throw RuntimeRangeError("Array buffer allocation failed");
		}
		auto ptr = Acquire();
		Local<ArrayBuffer> array_buffer = ArrayBuffer::New(Isolate::GetCurrent(), Length());
		std::memcpy(array_buffer->GetContents().Data(), ptr.get(), Length());
		return array_buffer;
	}
}

/**
 * ExternalCopySharedArrayBuffer implementation
 */
#if V8_AT_LEAST(7, 9, 69)

// This much needed feature was added in v8 55c48820
ExternalCopySharedArrayBuffer::ExternalCopySharedArrayBuffer(const v8::Local<v8::SharedArrayBuffer>& handle) :
		ExternalCopy{static_cast<int>(handle->ByteLength())} {
	backing_store = handle->GetBackingStore();
}

Local<Value> ExternalCopySharedArrayBuffer::CopyInto(bool /*transfer_in*/) {
	return SharedArrayBuffer::New(Isolate::GetCurrent(), backing_store);
}

#else

ExternalCopySharedArrayBuffer::ExternalCopySharedArrayBuffer(const v8::Local<v8::SharedArrayBuffer>& handle) :
		ExternalCopyBytes{handle->ByteLength() + sizeof(ExternalCopySharedArrayBuffer), [&]() -> std::shared_ptr<void> {
			// Inline lambda generates std::shared_ptr<void> for ExternalCopyBytes ctor. Similar to
			// ArrayBuffer::Transfer but different enough to make it not worth abstracting out..
			size_t length = handle->ByteLength();
			if (length == 0) {
				throw RuntimeGenericError("Array buffer is invalid");
			}
			if (handle->IsExternal()) {
				// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
				auto ptr = reinterpret_cast<Holder*>(handle->GetAlignedPointerFromInternalField(0));
				if (ptr == nullptr || ptr->magic != Holder::kMagic) { // dangerous pointer dereference
					throw RuntimeGenericError("Array buffer cannot be externalized");
				}
				// No race conditions here because only one thread can access `Holder`
				return ptr->cc_ptr;
			}
			// In this case the buffer is internal and should be externalized
			SharedArrayBuffer::Contents contents = handle->Externalize();
			auto value = std::shared_ptr<void>{contents.Data(), std::free};
			new Holder{handle, value, length};
			// Adjust allocator memory down, and `Holder` will adjust memory back up
			auto allocator = dynamic_cast<LimitedAllocator*>(IsolateEnvironment::GetCurrent()->GetAllocator());
			if (allocator != nullptr) {
				allocator->AdjustAllocatedSize(-static_cast<ptrdiff_t>(length));
			}
			return value;
		}(), handle->ByteLength()} {}

Local<Value> ExternalCopySharedArrayBuffer::CopyInto(bool /*transfer_in*/) {
	auto ptr = Acquire();
	Local<SharedArrayBuffer> array_buffer = SharedArrayBuffer::New(Isolate::GetCurrent(), ptr.get(), Length());
	new Holder{array_buffer, std::move(ptr), Length()};
	return array_buffer;
}

#endif

/**
 * ExternalCopyArrayBufferView implementation
 */
ExternalCopyArrayBufferView::ExternalCopyArrayBufferView(
	std::unique_ptr<ExternalCopyAnyArrayBuffer> buffer,
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

} // namespace ivm
