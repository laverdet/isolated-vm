#include "external_copy.h"

using namespace std;
using namespace v8;

namespace ivm {

unique_ptr<ExternalCopy> ExternalCopy::Copy(const Local<Value>& value) {
	unique_ptr<ExternalCopy> copy(CopyIfPrimitive(value));
	if (copy.get() != nullptr) {
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
		string name(*String::Utf8Value(object->GetConstructorName()));
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

			return make_unique<ExternalCopyError>(error_type, move(message_copy), move(stack_copy));
		}
	}
	return CopyIfPrimitive(value);
}

}
