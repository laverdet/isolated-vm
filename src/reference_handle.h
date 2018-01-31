#pragma once
#include <node.h>
#include "transferable_handle.h"
#include <memory>
#include <vector>

namespace ivm {

class DereferenceHandle;

/**
 * This will be a reference to any v8 Value in any isolate.
 */
class ReferenceHandle : public TransferableHandle {
	friend class Transferable;
	public:
		enum class TypeOf { Null, Undefined, Number, String, Boolean, Object, Function };

	private:
		class ReferenceHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<Persistent<Value>> reference;
				std::shared_ptr<Persistent<Context>> context;
				TypeOf type_of;

			public:
				ReferenceHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<Persistent<Value>> reference,
					std::shared_ptr<Persistent<Context>> context,
					TypeOf type_of
				);
				Local<Value> TransferIn() final;
		};

	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<Persistent<Value>> reference;
		std::shared_ptr<Persistent<Context>> context;
		TypeOf type_of;

		void CheckDisposed() const;

	public:
		ReferenceHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<Persistent<Value>> reference,
			std::shared_ptr<Persistent<Context>> context,
			TypeOf type_of
		);
		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific();
		static Local<FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
		static std::unique_ptr<ReferenceHandle> New(v8::Local<v8::Value> var);

		static void TypeOfGetter(Local<String> property, const PropertyCallbackInfo<Value>& info);
		v8::Local<v8::Value> Deref();
		v8::Local<v8::Value> DerefInto();
		v8::Local<v8::Value> Dispose();
		template <bool async> v8::Local<v8::Value> Copy();
		template <bool async> v8::Local<v8::Value> Get(v8::Local<v8::Value> key_handle);
		template <bool async> v8::Local<v8::Value> Set(v8::Local<v8::Value> key_handle, v8::Local<v8::Value> val_handle);
		template <bool async> v8::Local<v8::Value> Apply(
			v8::MaybeLocal<v8::Value> recv_handle,
			v8::MaybeLocal<v8::Array> maybe_arguments,
			v8::MaybeLocal<v8::Object> maybe_options
		);
};

/**
 * The return value for .derefInto()
 */
class DereferenceHandle : public TransferableHandle {
	friend class Transferable;

	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<Persistent<Value>> reference;

		class DereferenceHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<Persistent<Value>> reference;

			public:
				DereferenceHandleTransferable(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<Persistent<Value>> reference);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific();
		static Local<FunctionTemplate> Definition();
		DereferenceHandle(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<Persistent<Value>> reference);
		std::unique_ptr<Transferable> TransferOut() final;
};

}
