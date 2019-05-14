#pragma once
#include <v8.h>
#include "isolate/remote_handle.h"
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
	friend struct CopyRunner;
	friend struct GetRunner;
	friend struct SetRunner;
	friend struct ApplyRunner;
	public:
		enum class TypeOf { Null, Undefined, Number, String, Boolean, Object, Function };

	private:
		class ReferenceHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<RemoteHandle<v8::Value>> reference;
				std::shared_ptr<RemoteHandle<v8::Context>> context;
				TypeOf type_of;

			public:
				ReferenceHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<RemoteHandle<v8::Value>> reference,
					std::shared_ptr<RemoteHandle<v8::Context>> context,
					TypeOf type_of
				);
				v8::Local<v8::Value> TransferIn() final;
		};

	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<RemoteHandle<v8::Value>> reference;
		std::shared_ptr<RemoteHandle<v8::Context>> context;
		TypeOf type_of;

		void CheckDisposed() const;

	public:
		ReferenceHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<RemoteHandle<v8::Value>> reference,
			std::shared_ptr<RemoteHandle<v8::Context>> context,
			TypeOf type_of
		);
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
		static std::unique_ptr<ReferenceHandle> New(v8::Local<v8::Value> var);

		v8::Local<v8::Value> TypeOfGetter();
		v8::Local<v8::Value> Deref(v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> DerefInto(v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> Release();
		template <int async> v8::Local<v8::Value> Copy();
		template <int async> v8::Local<v8::Value> Get(v8::Local<v8::Value> key_handle);
		template <int async> v8::Local<v8::Value> Set(v8::Local<v8::Value> key_handle, v8::Local<v8::Value> val_handle);
		template <int async> v8::Local<v8::Value> Apply(
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
		std::shared_ptr<RemoteHandle<v8::Value>> reference;

		class DereferenceHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<RemoteHandle<v8::Value>> reference;

			public:
				DereferenceHandleTransferable(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<RemoteHandle<v8::Value>> reference);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		static v8::Local<v8::FunctionTemplate> Definition();
		DereferenceHandle(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<RemoteHandle<v8::Value>> reference);
		std::unique_ptr<Transferable> TransferOut() final;
};

} // namespace ivm
