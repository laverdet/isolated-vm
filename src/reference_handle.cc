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
	shared_ptr<RemoteHandle<Value>> reference,
	shared_ptr<RemoteHandle<Context>> context,
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
	shared_ptr<RemoteHandle<Value>> reference,
	shared_ptr<RemoteHandle<Context>> context,
	TypeOf type_of
) : isolate(std::move(isolate)), reference(std::move(reference)), context(std::move(context)), type_of(type_of) {}

Local<FunctionTemplate> ReferenceHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Reference", ParameterizeCtor<decltype(&New), &New>(),
		"deref", Parameterize<decltype(&ReferenceHandle::Deref), &ReferenceHandle::Deref>(),
		"derefInto", Parameterize<decltype(&ReferenceHandle::DerefInto), &ReferenceHandle::DerefInto>(),
		"release", Parameterize<decltype(&ReferenceHandle::Release), &ReferenceHandle::Release>(),
		"copy", Parameterize<decltype(&ReferenceHandle::Copy<1>), &ReferenceHandle::Copy<1>>(),
		"copySync", Parameterize<decltype(&ReferenceHandle::Copy<0>), &ReferenceHandle::Copy<0>>(),
		"get", Parameterize<decltype(&ReferenceHandle::Get<1>), &ReferenceHandle::Get<1>>(),
		"getSync", Parameterize<decltype(&ReferenceHandle::Get<0>), &ReferenceHandle::Get<0>>(),
		"set", Parameterize<decltype(&ReferenceHandle::Set<1>), &ReferenceHandle::Set<1>>(),
		"setIgnored", Parameterize<decltype(&ReferenceHandle::Set<2>), &ReferenceHandle::Set<2>>(),
		"setSync", Parameterize<decltype(&ReferenceHandle::Set<0>), &ReferenceHandle::Set<0>>(),
		"apply", Parameterize<decltype(&ReferenceHandle::Apply<1>), &ReferenceHandle::Apply<1>>(),
		"applyIgnored", Parameterize<decltype(&ReferenceHandle::Apply<2>), &ReferenceHandle::Apply<2>>(),
		"applySync", Parameterize<decltype(&ReferenceHandle::Apply<0>), &ReferenceHandle::Apply<0>>(),
		"applySyncPromise", Parameterize<decltype(&ReferenceHandle::Apply<4>), &ReferenceHandle::Apply<4>>(),
		"typeof", ParameterizeAccessor<decltype(&ReferenceHandle::TypeOfGetter), &ReferenceHandle::TypeOfGetter>()
	));
}

unique_ptr<Transferable> ReferenceHandle::TransferOut() {
	return std::make_unique<ReferenceHandleTransferable>(isolate, reference, context, type_of);
}

unique_ptr<ReferenceHandle> ReferenceHandle::New(Local<Value> var) {
	return std::make_unique<ReferenceHandle>(
		IsolateEnvironment::GetCurrentHolder(),
		std::make_shared<RemoteHandle<Value>>(var),
		std::make_shared<RemoteHandle<Context>>(Isolate::GetCurrent()->GetCurrentContext()),
		InferTypeOf(var)
	);
}

void ReferenceHandle::CheckDisposed() const {
	if (!reference) {
		throw js_generic_error("Reference has been released");
	}
}

/**
 * Getter for typeof property.
 */
Local<Value> ReferenceHandle::TypeOfGetter() {
	CheckDisposed();
	switch (type_of) {
		case TypeOf::Null:
			return v8_string("null");
		case TypeOf::Undefined:
			return v8_string("undefined");
		case TypeOf::Number:
			return v8_string("number");
		case TypeOf::String:
			return v8_string("string");
		case TypeOf::Boolean:
			return v8_string("boolean");
		case TypeOf::Object:
			return v8_string("object");
		case TypeOf::Function:
			return v8_string("function");
	}
	throw std::logic_error("msvc doesn't understand enums");
}

/**
 * Attempt to return this handle to the current context.
 */
Local<Value> ReferenceHandle::Deref(MaybeLocal<Object> maybe_options) {
	CheckDisposed();
	if (isolate.get() != IsolateEnvironment::GetCurrentHolder().get()) {
		throw js_type_error("Cannot dereference this from current isolate");
	}
	Local<Object> options;
	bool release = false;
	if (maybe_options.ToLocal(&options)) {
		release = IsOptionSet(Isolate::GetCurrent()->GetCurrentContext(), options, "release");
	}
	Local<Value> ret = ivm::Deref(*reference);
	if (release) {
		Release();
	}
	return ret;
}

/**
 * Return a handle which will dereference itself when passing into another isolate.
 */
Local<Value> ReferenceHandle::DerefInto(MaybeLocal<Object> maybe_options) {
	CheckDisposed();
	Local<Object> options;
	bool release = false;
	if (maybe_options.ToLocal(&options)) {
		release = IsOptionSet(Isolate::GetCurrent()->GetCurrentContext(), options, "release");
	}
	Local<Value> ret = ClassHandle::NewInstance<DereferenceHandle>(isolate, reference);
	if (release) {
		Release();
	}
	return ret;
}

/**
 * Release this reference.
 */
Local<Value> ReferenceHandle::Release() {
	CheckDisposed();
	isolate.reset();
	reference.reset();
	context.reset();
	return Undefined(Isolate::GetCurrent());
}

/**
 * Copy this reference's value into this isolate
 */
struct CopyRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<RemoteHandle<Value>> reference;
	unique_ptr<Transferable> copy;

	CopyRunner(
		const ReferenceHandle& that,
		shared_ptr<RemoteHandle<Context>> context,
		shared_ptr<RemoteHandle<Value>> reference
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

template <int async>
Local<Value> ReferenceHandle::Copy() {
	return ThreePhaseTask::Run<async, CopyRunner>(*isolate, *this, context, reference);
}

/**
 * Get a property from this reference, returned as another reference
 */
struct GetRunner : public ThreePhaseTask {
	unique_ptr<ExternalCopy> key;
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<RemoteHandle<Value>> reference;
	unique_ptr<ReferenceHandle::ReferenceHandleTransferable> ret;

	GetRunner(
		const ReferenceHandle& that,
		Local<Value>& key_handle,
		shared_ptr<RemoteHandle<Context>> context,
		shared_ptr<RemoteHandle<Value>> reference
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
		ret = std::make_unique<ReferenceHandle::ReferenceHandleTransferable>(
			IsolateEnvironment::GetCurrentHolder(),
			std::make_shared<RemoteHandle<Value>>(value),
			context,
			InferTypeOf(value)
		);
	}

	Local<Value> Phase3() final {
		return ret->TransferIn();
	}
};
template <int async>
Local<Value> ReferenceHandle::Get(Local<Value> key_handle) {
	return ThreePhaseTask::Run<async, GetRunner>(*isolate, *this, key_handle, context, reference);
}

/**
 * Attempt to set a property on this reference
 */
struct SetRunner : public ThreePhaseTask {
	unique_ptr<ExternalCopy> key;
	unique_ptr<Transferable> val;
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<RemoteHandle<Value>> reference;
	bool did_set = false;

	SetRunner(
		ReferenceHandle& that,
		Local<Value>& key_handle,
		Local<Value>& val_handle,
		shared_ptr<RemoteHandle<Context>> context,
		shared_ptr<RemoteHandle<Value>> reference
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
		Local<Object> object = Local<Object>::Cast(ivm::Deref(*reference));
		// Delete key before transferring in, potentially freeing up some v8 heap
		Unmaybe(object->Delete(context_handle, key_inner));
		Local<Value> val_inner = val->TransferIn();
		did_set = Unmaybe(object->Set(context_handle, key_inner, val_inner));
	}

	Local<Value> Phase3() final {
		return Boolean::New(Isolate::GetCurrent(), did_set);
	}
};
template <int async>
Local<Value> ReferenceHandle::Set(Local<Value> key_handle, Local<Value> val_handle) {
	return ThreePhaseTask::Run<async, SetRunner>(*isolate, *this, key_handle, val_handle, context, reference);
}

/**
 * Call a function, like Function.prototype.apply
 */
struct ApplyRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<RemoteHandle<Value>> reference;
	unique_ptr<Transferable> recv;
	std::vector<unique_ptr<Transferable>> argv;
	uint32_t timeout = 0;
	unique_ptr<Transferable> ret;
	// Only used in the AsyncPhase2 case
	shared_ptr<bool> did_finish;
	IsolateEnvironment::Scheduler::AsyncWait* async_wait = nullptr;
	unique_ptr<ExternalCopy> async_error;

	ApplyRunner(
		ReferenceHandle& that,
		MaybeLocal<Value>& recv_handle,
		MaybeLocal<Array>& maybe_arguments,
		MaybeLocal<Object>& maybe_options,
		shared_ptr<RemoteHandle<Context>> context,
		shared_ptr<RemoteHandle<Value>> reference
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

	/**
	 * This is an internal callback that will be called after a Promise returned from
	 * `applySyncPromise` has resolved
	 */
	static void AsyncCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
		// It's possible the invocation timed out, in which case the ApplyRunner will be dead. The
		// shared_ptr<bool> here will be marked as true and we can exit early.
		unique_ptr<shared_ptr<bool>> did_finish(reinterpret_cast<shared_ptr<bool>*>(info[1].As<External>()->Value()));
		if (**did_finish) {
			return;
		}
		ApplyRunner& self = *reinterpret_cast<ApplyRunner*>(info[0].As<External>()->Value());
		if (info.Length() == 3) {
			// Resolved
			FunctorRunners::RunCatchExternal(IsolateEnvironment::GetCurrent()->DefaultContext(), [ &self, &info ]() {
				self.ret = Transferable::TransferOut(info[2]);
			}, [ &self ](unique_ptr<ExternalCopy> error) {
				self.async_error = std::move(error);
			});
		} else {
			// Rejected
			self.async_error = ExternalCopy::CopyIfPrimitiveOrError(info[3]);
			if (!self.async_error) {
				self.async_error = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error,
					"An object was thrown from supplied code within isolated-vm, but that object was not an instance of `Error`."
				);
			}
		}
		*self.did_finish = true;
		self.async_wait->Wake();
	}

	/**
	 * The C++ promise interface is a little clumsy so this does some work in JS for us. This function
	 * is called once and returns a JS function that will be reused.
	 */
	static Local<Function> CompileAsyncWrapper() {
		Isolate* isolate = Isolate::GetCurrent();
		Local<Context> context = IsolateEnvironment::GetCurrent()->DefaultContext();
		Local<Script> script = Unmaybe(Script::Compile(context, v8_string(
			"'use strict';"
			"(function(AsyncCallback) {"
				"return function(ptr, did_finish, promise) {"
					"promise.then(function(val) {"
						"AsyncCallback(ptr, did_finish, val);"
					"}, function(err) {"
						"AsyncCallback(ptr, did_finish, null, err);"
					"});"
				"};"
			"})"
		)));
		Local<Value> outer_fn = Unmaybe(script->Run(context));
		assert(outer_fn->IsFunction());
		Local<Value> callback_fn = Unmaybe(FunctionTemplate::New(isolate, AsyncCallback)->GetFunction(context));
		Local<Value> inner_fn = Unmaybe(outer_fn.As<Function>()->Call(context, Undefined(isolate), 1, &callback_fn));
		assert(inner_fn->IsFunction());
		return inner_fn.As<Function>();
	}

	std::vector<Local<Value>> TransferArguments() {
		std::vector<Local<Value>> argv_inner;
		size_t argc = argv.size();
		argv_inner.reserve(argc);
		for (size_t ii = 0; ii < argc; ++ii) {
			argv_inner.emplace_back(argv[ii]->TransferIn());
		}
		return argv_inner;
	}

	void Phase2() final {
		// Invoke in the isolate
		Local<Context> context_handle = ivm::Deref(*context);
		Context::Scope context_scope(context_handle);
		Local<Value> fn = ivm::Deref(*reference);
		if (!fn->IsFunction()) {
			throw js_type_error("Reference is not a function");
		}
		std::vector<Local<Value>> argv_inner = TransferArguments();
		Local<Value> recv_inner = recv->TransferIn();
		ret = Transferable::TransferOut(RunWithTimeout(
			timeout,
			[&fn, &context_handle, &recv_inner, &argv_inner]() {
				return fn.As<Function>()->Call(context_handle, recv_inner, argv_inner.size(), argv_inner.empty() ? nullptr : &argv_inner[0]);
			}
		));
	}

	bool Phase2Async(IsolateEnvironment::Scheduler::AsyncWait& wait) final {
		// Same as regular `Phase2()` but if it returns a promise we will wait on it
		Local<Context> context_handle = ivm::Deref(*context);
		Context::Scope context_scope(context_handle);
		Local<Value> fn = ivm::Deref(*reference);
		if (!fn->IsFunction()) {
			throw js_type_error("Reference is not a function");
		}
		Local<Value> recv_inner = recv->TransferIn();
		std::vector<Local<Value>> argv_inner = TransferArguments();
		Local<Value> value = RunWithTimeout(
			timeout,
			[&fn, &context_handle, &recv_inner, &argv_inner]() {
				return fn.As<Function>()->Call(context_handle, recv_inner, argv_inner.size(), argv_inner.empty() ? nullptr : &argv_inner[0]);
			}
		);
		if (value->IsPromise()) {
			Isolate* isolate = Isolate::GetCurrent();
			// This is only called from the default isolate, so we don't need an IsolateSpecific
			static Persistent<Function> callback_persistent(isolate, CompileAsyncWrapper());
			Local<Function> callback_fn = Deref(callback_persistent);
			did_finish = std::make_shared<bool>(false);
			Local<Value> argv[3];
			argv[0] = External::New(isolate, reinterpret_cast<void*>(this));
			argv[1] = External::New(isolate, reinterpret_cast<void*>(new std::shared_ptr<bool>(did_finish)));
			argv[2] = value;
			this->async_wait = &wait;
			Unmaybe(callback_fn->Call(context_handle, callback_fn, 3, argv));
			return true;
		} else {
			ret = Transferable::TransferOut(value);
			return false;
		}
	}

	Local<Value> Phase3() final {
		if (did_finish && !*did_finish) {
			*did_finish = true;
			throw js_generic_error("Script execution timed out.");
		} else if (async_error) {
			Isolate::GetCurrent()->ThrowException(async_error->CopyInto());
			throw js_runtime_error();
		} else {
			return ret->TransferIn();
		}
	}
};
template <int async>
Local<Value> ReferenceHandle::Apply(MaybeLocal<Value> recv_handle, MaybeLocal<Array> maybe_arguments, MaybeLocal<Object> maybe_options) {
	return ThreePhaseTask::Run<async, ApplyRunner>(*isolate, *this, recv_handle, maybe_arguments, maybe_options, context, reference);
}

/**
 * DereferenceHandle implementation
 */
DereferenceHandle::DereferenceHandleTransferable::DereferenceHandleTransferable(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<RemoteHandle<Value>> reference
) : isolate(std::move(isolate)), reference(std::move(reference)) {}

Local<Value> DereferenceHandle::DereferenceHandleTransferable::TransferIn() {
	if (isolate == IsolateEnvironment::GetCurrentHolder()) {
		return Deref(*reference);
	} else {
		throw js_type_error("Cannot dereference this into target isolate");
	}
}

Local<FunctionTemplate> DereferenceHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass("Dereference", nullptr));
}

DereferenceHandle::DereferenceHandle(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<RemoteHandle<Value>> reference
) : isolate(std::move(isolate)), reference(std::move(reference)) {}

unique_ptr<Transferable> DereferenceHandle::TransferOut() {
	if (!reference) {
		throw js_generic_error("The return value of `derefInto()` should only be used once");
	}
	return std::make_unique<DereferenceHandleTransferable>(std::move(isolate), std::move(reference));
}

} // namespace ivm
