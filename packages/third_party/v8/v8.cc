module;
#include "v8.h"
#include "version.h"
#ifdef INCLUDE_V8_PLATFORM
#include "libplatform/libplatform.h"
#include "v8-platform.h"
#endif
export module v8;

// NOLINTBEGIN(misc-unused-using-decls)
export namespace v8 {

// v8-array-buffer.h
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::DataView;
using v8::SharedArrayBuffer;

// v8-container.h
using v8::Array;

// v8-context.h
using v8::Context;

// v8-data.h
using v8::Data;
using v8::FixedArray;

// v8-date.h
using v8::Date;

// v8-debug.h
using v8::StackFrame;
using v8::StackTrace;

// v8-exception.h
using v8::Exception;
using v8::TryCatch;

// v8-external.h
using v8::External;

// v8-function-callback.h
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::PropertyCallbackInfo;
using v8::ReturnValue;

// v8-function.h
using v8::Function;

// v8-initialization.h
using v8::DcheckErrorCallback;
using v8::EntropySource;
using v8::ReturnAddressLocationResolver;
using v8::V8;
using v8::V8FatalErrorCallback;

// v8-isolate.h
using v8::Isolate;

// v8-local-handle.h
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;

// v8-locker.h
using v8::Locker;
using v8::Unlocker;

// v8-maybe.h
using v8::Maybe;

// v8-memory-span.h
using v8::MemorySpan;

// v8-message.h
using v8::Message;
using v8::ScriptOrigin;
using v8::ScriptOriginOptions;

// v8-object.h
using v8::IndexFilter;
using v8::KeyCollectionMode;
using v8::KeyConversionMode;
using v8::Object;
using v8::PropertyFilter;

// v8-persistent-handle.h
using v8::Global;
using v8::Persistent;

// v8-primitive.h
using v8::BigInt;
using v8::Boolean;
using v8::Int32;
using v8::Integer;
using v8::Name;
using v8::NewStringType;
using v8::Number;
#if V8_HAS_NUMERIC
using v8::Numeric;
#endif
using v8::Null;
using v8::Primitive;
using v8::String;
using v8::Symbol;
using v8::Uint32;
using v8::Undefined;

// v8-promise.h
using v8::Promise;
using v8::PromiseHook;
using v8::PromiseHookType;
using v8::PromiseRejectCallback;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;

// v8-script.h
using v8::Location;
using v8::Module;
using v8::ModuleRequest;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrModule;
using v8::ScriptType;
using v8::UnboundModuleScript;
using v8::UnboundScript;

// v8-source-location.h
using v8::SourceLocation;

// v8-template.h
using v8::AccessCheckCallback;
using v8::ConstructorBehavior;
using v8::DictionaryTemplate;
using v8::FunctionTemplate;
using v8::GenericNamedPropertyDefinerCallback;
using v8::GenericNamedPropertyDeleterCallback;
using v8::GenericNamedPropertyDescriptorCallback;
using v8::GenericNamedPropertyEnumeratorCallback;
using v8::GenericNamedPropertyQueryCallback;
using v8::GenericNamedPropertySetterCallback;
using v8::IndexedPropertyDefinerCallback;
using v8::IndexedPropertyDefinerCallbackV2;
using v8::IndexedPropertyDeleterCallback;
using v8::IndexedPropertyDeleterCallbackV2;
using v8::IndexedPropertyDescriptorCallback;
using v8::IndexedPropertyDescriptorCallbackV2;
using v8::IndexedPropertyEnumeratorCallback;
using v8::IndexedPropertyGetterCallback;
using v8::IndexedPropertyGetterCallbackV2;
using v8::IndexedPropertyHandlerConfiguration;
using v8::IndexedPropertyQueryCallback;
using v8::IndexedPropertyQueryCallbackV2;
using v8::IndexedPropertySetterCallback;
using v8::IndexedPropertySetterCallbackV2;
using v8::Intercepted;
using v8::NamedPropertyDefinerCallback;
using v8::NamedPropertyDeleterCallback;
using v8::NamedPropertyDescriptorCallback;
using v8::NamedPropertyEnumeratorCallback;
using v8::NamedPropertyHandlerConfiguration;
using v8::NamedPropertyQueryCallback;
using v8::NamedPropertySetterCallback;
using v8::ObjectTemplate;
using v8::PropertyHandlerFlags;
using v8::Signature;
using v8::Template;

// v8-typed-array.h
using v8::BigInt64Array;
using v8::BigUint64Array;
using v8::Float16Array;
using v8::Float32Array;
using v8::Float64Array;
using v8::Int16Array;
using v8::Int32Array;
using v8::Int8Array;
using v8::TypedArray;
using v8::Uint16Array;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Uint8ClampedArray;

// v8-value.h
using v8::Value;
using v8::WeakCallbackInfo;
using v8::WeakCallbackType;

#ifdef INCLUDE_V8_PLATFORM
// libplatform/libplatform.h
namespace platform {
using platform::NewDefaultJobHandle;
using platform::NewDefaultPlatform;
} // namespace platform

// v8-platform.h
using v8::IdleTask;
using v8::JobDelegate;
using v8::JobHandle;
using v8::JobTask;
using v8::PageAllocator;
using v8::Platform;
using v8::Task;
using v8::TaskPriority;
using v8::TaskRunner;
using v8::TracingController;
namespace platform::tracing {
using tracing::TracingController;
}
#endif

} // namespace v8
// NOLINTEND(misc-unused-using-decls)
