#pragma once
#include <v8.h>
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ExternalCopy;

class ExternalCopyHandle : public TransferableHandle {
	private:
		class ExternalCopyTransferable : public Transferable {
			private:
				std::shared_ptr<ExternalCopy> value;

			public:
				explicit ExternalCopyTransferable(std::shared_ptr<ExternalCopy> value);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<ExternalCopy> value;

		void CheckDisposed();

	public:
		explicit ExternalCopyHandle(std::shared_ptr<ExternalCopy> value);
		ExternalCopyHandle(const ExternalCopyHandle&) = delete;
		ExternalCopyHandle& operator= (const ExternalCopyHandle&) = delete;
		~ExternalCopyHandle() final;
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		static std::unique_ptr<ExternalCopyHandle> New(v8::Local<v8::Value> value, v8::MaybeLocal<v8::Object> maybe_options);
		static v8::Local<v8::Value> TotalExternalSizeGetter();
		v8::Local<v8::Value> Copy(v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> CopyInto(v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> Release();
		std::shared_ptr<ExternalCopy> GetValue() const { return value; }
};

class ExternalCopyIntoHandle : public TransferableHandle {
	private:
		class ExternalCopyIntoTransferable : public Transferable {
			private:
				std::shared_ptr<ExternalCopy> value;
				bool transfer_in;

			public:
				explicit ExternalCopyIntoTransferable(std::shared_ptr<ExternalCopy> value, bool transfer_in);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<ExternalCopy> value;
		bool transfer_in;

	public:
		explicit ExternalCopyIntoHandle(std::shared_ptr<ExternalCopy> value, bool transfer_in);
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
};

} // namespace ivm
