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
		~ExternalCopyHandle() final;
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		static std::unique_ptr<ExternalCopyHandle> New(v8::Local<v8::Value> value);
		v8::Local<v8::Value> Copy();
		v8::Local<v8::Value> CopyInto();
		v8::Local<v8::Value> Dispose();
		std::shared_ptr<ExternalCopy> GetValue() const { return value; }
};

class ExternalCopyIntoHandle : public TransferableHandle {
	private:
		class ExternalCopyIntoTransferable : public Transferable {
			private:
				std::shared_ptr<ExternalCopy> value;

			public:
				ExternalCopyIntoTransferable(std::shared_ptr<ExternalCopy> value);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<ExternalCopy> value;

	public:
		ExternalCopyIntoHandle(std::shared_ptr<ExternalCopy> value);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut();
};

} // namespace ivm
