#include "reference_handle.h"
#include "external_copy.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

using TypeOf = ReferenceHandle::TypeOf;
static TypeOf InferTypeOf(Local<Value> value) {
	if (value->IsNull()) {
		return TypeOf::Null;
	} else if (value->IsUndefined()) {
		return TypeOf::Undefined;
	} else if (value->IsNumber()) {
		return TypeOf::Number;
	} else if (value->IsString()) {
		return TypeOf::String;
	} else if (value->IsBoolean()) {
		return TypeOf::Boolean;
	} else if (value->IsFunction()) {
		return TypeOf::Function;
	} else {
		return TypeOf::Object;
	}
}

/**
 * ReferenceHandleTransferable implementation
 */
ReferenceHandle::ReferenceHandleTransferable::ReferenceHandleTransferable(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Value>> reference,
	shared_ptr<Persistent<Context>> context,
	TypeOf type_of
) : isolate(std::move(isolate)), reference(std::move(reference)), context(std::move(context)), type_of(type_of) {}

Local<Value> ReferenceHandle::ReferenceHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ReferenceHandle>(isolate, reference, context, type_of);
}

/**
 * ReferenceHandle implementation
 */
ReferenceHandle::ReferenceHandle(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Value>> reference,
	shared_ptr<Persistent<Context>> context,
	TypeOf type_of
) : isolate(std::move(isolate)), reference(std::move(reference)), context(std::move(context)), type_of(type_of) {}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& ReferenceHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> ReferenceHandle::Definition() {
	Local<FunctionTemplate> tmpl = Inherit<TransferableHandle>(MakeClass(
		"Reference", ParameterizeCtor<decltype(&New), &New>, 1,
		"deref", Parameterize<decltype(&ReferenceHandle::Deref), &ReferenceHandle::Deref>, 0,
		"derefInto", Parameterize<decltype(&ReferenceHandle::DerefInto), &ReferenceHandle::DerefInto>, 0,
		"dispose", Parameterize<decltype(&ReferenceHandle::Dispose), &ReferenceHandle::Dispose>, 0,
		"copy", Parameterize<decltype(&ReferenceHandle::Copy<true>), &ReferenceHandle::Copy<true>>, 1,
		"copySync", Parameterize<decltype(&ReferenceHandle::Copy<false>), &ReferenceHandle::Copy<false>>, 1,
		"get", Parameterize<decltype(&ReferenceHandle::Get<true>), &ReferenceHandle::Get<true>>, 1,
		"getSync", Parameterize<decltype(&ReferenceHandle::Get<false>), &ReferenceHandle::Get<false>>, 1,
		"set", Parameterize<decltype(&ReferenceHandle::Set<true>), &ReferenceHandle::Set<true>>, 2,
		"setSync", Parameterize<decltype(&ReferenceHandle::Set<false>), &ReferenceHandle::Set<false>>, 2,
		"apply", Parameterize<decltype(&ReferenceHandle::Apply<true>), &ReferenceHandle::Apply<true>>, 2,
		"applySync", Parameterize<decltype(&ReferenceHandle::Apply<false>), &ReferenceHandle::Apply<false>>, 2
	));
	tmpl->InstanceTemplate()->SetAccessor(v8_symbol("typeof"), TypeOfGetter, nullptr, Local<Value>(), ALL_CAN_READ);
	return tmpl;
}

unique_ptr<Transferable> ReferenceHandle::TransferOut() {
	return std::make_unique<ReferenceHandleTransferable>(isolate, reference, context, type_of);
}

unique_ptr<ReferenceHandle> ReferenceHandle::New(Local<Value> var) {
	Isolate* isolate = Isolate::GetCurrent();
	return std::make_unique<ReferenceHandle>(
		IsolateEnvironment::GetCurrentHolder(),
		std::make_shared<Persistent<Value>>(isolate, var),
		std::make_shared<Persistent<Context>>(isolate, isolate->GetCurrentContext()),
		InferTypeOf(var)
	);
}

void ReferenceHandle::CheckDisposed() const {
	if (!reference) {
		throw js_generic_error("Reference is disposed");
	}
}

/**
 * Getter for typeof property.
 */
void ReferenceHandle::TypeOfGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
	// TODO: This will crash if someone steals the getter and applies another object to it
	ReferenceHandle& that = *dynamic_cast<ReferenceHandle*>(Unwrap(info.This()));
	that.CheckDisposed();
	switch (that.type_of) {
		case TypeOf::Null:
			info.GetReturnValue().Set(v8_string("null"));
			break;
		case TypeOf::Undefined:
			info.GetReturnValue().Set(v8_string("undefined"));
			break;
		case TypeOf::Number:
			info.GetReturnValue().Set(v8_string("number"));
			break;
		case TypeOf::String:
			info.GetReturnValue().Set(v8_string("string"));
			break;
		case TypeOf::Boolean:
			info.GetReturnValue().Set(v8_string("boolean"));
			break;
		case TypeOf::Object:
			info.GetReturnValue().Set(v8_string("object"));
			break;
		case TypeOf::Function:
			info.GetReturnValue().Set(v8_string("function"));
			break;
	}
}

/**
 * Attempt to return this handle to the current context.
 */
Local<Value> ReferenceHandle::Deref() {
	CheckDisposed();
	if (isolate.get() != IsolateEnvironment::GetCurrentHolder().get()) {
		throw js_type_error("Cannot dereference this from current isolate");
	}
	return ivm::Deref(*reference);
}

/**
 * Return a handle which will dereference itself when passing into another isolate.
 */
Local<Value> ReferenceHandle::DerefInto() {
	CheckDisposed();
	return ClassHandle::NewInstance<DereferenceHandle>(isolate, reference);
}

/**
 * Release this reference.
 */
Local<Value> ReferenceHandle::Dispose() {
	CheckDisposed();
	isolate.reset();
	reference.reset();
	context.reset();
	return Undefined(Isolate::GetCurrent());
}

/**
 * Copy this reference's value into this isolate
 */
template <bool async>
Local<Value> ReferenceHandle::Copy() {
	struct Copy : public ThreePhaseTask {
		shared_ptr<Persistent<Context>> context;
		shared_ptr<Persistent<Value>> reference;
		unique_ptr<Transferable> copy;

		Copy(
			const ReferenceHandle& that,
			shared_ptr<Persistent<Context>> context,
			shared_ptr<Persistent<Value>> reference
		) : context(std::move(context)), reference(std::move(reference)) {
			that.CheckDisposed();
		}

		void Phase2() final {
			Context::Scope context_scope(ivm::Deref(*context));
			Local<Value> value = ivm::Deref(*reference);
			copy = ExternalCopy::Copy(value);
		}

		Local<Value> Phase3() final {
			return copy->TransferIn();
		}
	};
	return ThreePhaseTask::Run<async, Copy>(*isolate, *this, context, reference);
}

/**
 * Get a property from this reference, returned as another reference
 */
template <bool async>
Local<Value> ReferenceHandle::Get(Local<Value> key_handle) {
	struct Get : public ThreePhaseTask {
		unique_ptr<ExternalCopy> key;
		shared_ptr<Persistent<Context>> context;
		shared_ptr<Persistent<Value>> reference;
		unique_ptr<ReferenceHandleTransferable> ret;

		Get(
			const ReferenceHandle& that,
			Local<Value>& key_handle,
			shared_ptr<Persistent<Context>> context,
			shared_ptr<Persistent<Value>> reference
		) : context(std::move(context)), reference(std::move(reference)) {
			that.CheckDisposed();
			key = ExternalCopy::CopyIfPrimitive(key_handle);
			if (!key) {
				throw js_type_error("Invalid `key`");
			}
		}

		void Phase2() final {
			Local<Context> context_handle = ivm::Deref(*context);
			Context::Scope context_scope(context_handle);
			Local<Value> key_inner = key->CopyInto();
			Local<Object> object = Local<Object>::Cast(ivm::Deref(*reference));
			Local<Value> value = Unmaybe(object->Get(context_handle, key_inner));
			ret = std::make_unique<ReferenceHandleTransferable>(
				IsolateEnvironment::GetCurrentHolder(),
				std::make_shared<Persistent<Value>>(Isolate::GetCurrent(), value),
				context,
				InferTypeOf(value)
			);
		}

		Local<Value> Phase3() final {
			return ret->TransferIn();
		}
	};
	return ThreePhaseTask::Run<async, Get>(*isolate, *this, key_handle, context, reference);
}

/**
 * Attempt to set a property on this reference
 */
template <bool async>
Local<Value> ReferenceHandle::Set(Local<Value> key_handle, Local<Value> val_handle) {
	struct Set : public ThreePhaseTask {
		unique_ptr<ExternalCopy> key;
		unique_ptr<Transferable> val;
		shared_ptr<Persistent<Context>> context;
		shared_ptr<Persistent<Value>> reference;
		bool did_set;

		Set(
			ReferenceHandle& that,
			Local<Value>& key_handle,
			Local<Value>& val_handle,
			shared_ptr<Persistent<Context>> context,
			shared_ptr<Persistent<Value>> reference
		) :	context(std::move(context)), reference(std::move(reference)) {
			that.CheckDisposed();
			key = ExternalCopy::CopyIfPrimitive(key_handle);
			if (!key) {
				throw js_type_error("Invalid `key`");
			}
			val = Transferable::TransferOut(val_handle);
		}

		void Phase2() final {
			Local<Context> context_handle = ivm::Deref(*context);
			Context::Scope context_scope(context_handle);
			Local<Value> key_inner = key->CopyInto();
			Local<Value> val_inner = val->TransferIn();
			Local<Object> object = Local<Object>::Cast(ivm::Deref(*reference));
			did_set = Unmaybe(object->Set(context_handle, key_inner, val_inner));
		}

		Local<Value> Phase3() final {
			return Boolean::New(Isolate::GetCurrent(), did_set);
		}
	};
	return ThreePhaseTask::Run<async, Set>(*isolate, *this, key_handle, val_handle, context, reference);
}

/**
 * Call a function, like Function.prototype.apply
 */
template <bool async>
Local<Value> ReferenceHandle::Apply(MaybeLocal<Value> recv_handle, MaybeLocal<Array> maybe_arguments, MaybeLocal<Object> maybe_options) {
	struct Apply : public ThreePhaseTask {
		shared_ptr<Persistent<Context>> context;
		shared_ptr<Persistent<Value>> reference;
		unique_ptr<Transferable> recv;
		std::vector<unique_ptr<Transferable>> argv;
		uint32_t timeout = 0;
		unique_ptr<Transferable> ret;

		Apply(
			ReferenceHandle& that,
			MaybeLocal<Value>& recv_handle,
			MaybeLocal<Array>& maybe_arguments,
			MaybeLocal<Object>& maybe_options,
			shared_ptr<Persistent<Context>> context,
			shared_ptr<Persistent<Value>> reference
		) :	context(std::move(context)), reference(std::move(reference))
		{
			that.CheckDisposed();

			// Get receiver, holder, this, whatever
			if (!recv_handle.IsEmpty()) {
				recv = Transferable::TransferOut(recv_handle.ToLocalChecked());
			}

			// Externalize all arguments
			if (!maybe_arguments.IsEmpty()) {
				Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
				Local<Array> arguments = maybe_arguments.ToLocalChecked();
				Local<Array> keys = Unmaybe(arguments->GetOwnPropertyNames(context));
				argv.reserve(keys->Length());
				for (uint32_t ii = 0; ii < keys->Length(); ++ii) {
					Local<Uint32> key = Unmaybe(Unmaybe(keys->Get(context, ii))->ToArrayIndex(context));
					if (key->Value() != ii) {
						throw js_type_error("Invalid `arguments` array");
					}
					argv.push_back(Transferable::TransferOut(Unmaybe(arguments->Get(context, key))));
				}
			}

			// Get run options
			Local<Object> options;
			if (maybe_options.ToLocal(&options)) {
				Local<Value> timeout_handle = Unmaybe(options->Get(Isolate::GetCurrent()->GetCurrentContext(), v8_string("timeout")));
				if (!timeout_handle->IsUndefined()) {
					if (!timeout_handle->IsUint32()) {
						throw js_type_error("`timeout` must be integer");
					}
					timeout = timeout_handle.As<Uint32>()->Value();
				}
			}
		}

		void Phase2() final {
			// Invoke in the isolate
			Local<Context> context_handle = ivm::Deref(*context);
			Context::Scope context_scope(context_handle);
			Local<Value> fn = ivm::Deref(*reference);
			if (!fn->IsFunction()) {
				throw js_type_error("Reference is not a function");
			}
			Local<Value> recv_inner = recv->TransferIn();
			std::vector<Local<Value>> argv_inner;
			size_t argc = argv.size();
			argv_inner.reserve(argc);
			for (size_t ii = 0; ii < argc; ++ii) {
				argv_inner.emplace_back(argv[ii]->TransferIn());
			}
			ret = Transferable::TransferOut(RunWithTimeout(
				timeout,
				[&fn, &context_handle, &recv_inner, argc, &argv_inner]() {
					return fn.As<Function>()->Call(context_handle, recv_inner, argc, argc ? &argv_inner[0] : nullptr);
				}
			));
		}

		Local<Value> Phase3() final {
			return ret->TransferIn();
		}
	};
	return ThreePhaseTask::Run<async, Apply>(*isolate, *this, recv_handle, maybe_arguments, maybe_options, context, reference);
}

/**
 * DereferenceHandle implementation
 */
DereferenceHandle::DereferenceHandleTransferable::DereferenceHandleTransferable(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Value>> reference
) : isolate(std::move(isolate)), reference(std::move(reference)) {}

Local<Value> DereferenceHandle::DereferenceHandleTransferable::TransferIn() {
	if (isolate == IsolateEnvironment::GetCurrentHolder()) {
		return Deref(*reference);
	} else {
		throw js_type_error("Cannot dereference this into target isolate");
	}
}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& DereferenceHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> DereferenceHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass("Dereference", nullptr, 0));
}

DereferenceHandle::DereferenceHandle(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Value>> reference
) : isolate(std::move(isolate)), reference(std::move(reference)) {}

unique_ptr<Transferable> DereferenceHandle::TransferOut() {
	return std::make_unique<DereferenceHandleTransferable>(isolate, reference);
}

} // namespace ivm
