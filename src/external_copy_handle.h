#pragma once
#include <node.h>
#include "shareable_persistent.h"
#include "class_handle.h"
#include "transferable.h"
#include "transferable_handle.h"
#include "external_copy.h"

#include <memory>

namespace ivm {
using namespace v8;
using std::shared_ptr;

class ExternalCopyIntoHandle;

class ExternalCopyHandle : public TransferableHandle {
	private:
		shared_ptr<ExternalCopy> value;

		/**
		 * Wrapper to transfer this ExternalCopy handle into another ExternalCopy handle in another
		 * isolate. ExternalCopyIntoHandle is the one that throws away the ExternalCopy reference and
		 * returns the copy's value.
		 */
		class ExternalCopyTransferable : public Transferable {
			private:
				shared_ptr<ExternalCopy> value;

			public:
				ExternalCopyTransferable(shared_ptr<ExternalCopy>& value) : value(value) {}

				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ExternalCopyHandle>(value);
				}
		};

		/**
		 * Just throws if this handle is disposed
		 */
		void CheckDisposed() {
			if (value.get() == nullptr) {
				throw js_generic_error("Copy is disposed");
			}
		}

	public:
		ExternalCopyHandle(shared_ptr<ExternalCopy> value) : value(value) {
			Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(value->Size());
		}

		virtual ~ExternalCopyHandle() {
			if (value.get() != nullptr) {
				Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-value->Size());
			}
		}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"ExternalCopy", Parameterize<decltype(New), New>, 1,
				"copy", Parameterize<decltype(&ExternalCopyHandle::Copy), &ExternalCopyHandle::Copy>, 0,
				"copyInto", Parameterize<decltype(&ExternalCopyHandle::CopyInto), &ExternalCopyHandle::CopyInto>, 0,
				"dispose", Parameterize<decltype(&ExternalCopyHandle::Dispose), &ExternalCopyHandle::Dispose>, 0
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ExternalCopyTransferable>(value);
		}

		static unique_ptr<ExternalCopyHandle> New(Local<Value> value) {
			return std::make_unique<ExternalCopyHandle>(shared_ptr<ExternalCopy>(ExternalCopy::Copy(value)));
		}

		Local<Value> Copy() {
			CheckDisposed();
			return value->CopyInto();
		}

		Local<Value> CopyInto() {
			CheckDisposed();
			return ClassHandle::NewInstance<ExternalCopyIntoHandle>(value);
		}

		Local<Value> Dispose() {
			CheckDisposed();
			Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-value->Size());
			value.reset();
			return Undefined(Isolate::GetCurrent());
		}

		shared_ptr<ExternalCopy> Value() {
			return value;
		}
};

class ExternalCopyIntoHandle : public TransferableHandle {
	private:
		shared_ptr<ExternalCopy> value;

		/**
		 * Wrapper that calls CopyInto in the target isolate. We could actually just return an
		 * ExternalCopy since that inherits from Transferable as well but we need a unique_ptr and we
		 * have a shared_ptr here.
		 */
		class ExternalCopyIntoTransferable : public Transferable {
			private:
				shared_ptr<ExternalCopy> value;

			public:
				ExternalCopyIntoTransferable(shared_ptr<ExternalCopy>& value) : value(value) {}
				virtual Local<Value> TransferIn() {
					return value->CopyInto();
				}
		};

	public:
		ExternalCopyIntoHandle(shared_ptr<ExternalCopy>& value) : value(value) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass("ExternalCopyInto", nullptr, 0));
		}

		unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ExternalCopyIntoTransferable>(value);
		}

};

}
