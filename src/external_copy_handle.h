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

	public:
		ExternalCopyHandle(shared_ptr<ExternalCopy>& value) : value(value) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"ExternalCopy", New, 1,
				"copy", Method<ExternalCopyHandle, &ExternalCopyHandle::Copy>, 0,
				"copyInto", Method<ExternalCopyHandle, &ExternalCopyHandle::CopyInto>, 0
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ExternalCopyTransferable>(value);
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			if (!args.IsConstructCall()) {
				THROW(Exception::TypeError, "Class constructor ExternalCopy cannot be invoked without 'new'");
			} else if (args.Length() < 1) {
				THROW(Exception::TypeError, "Class constructor ExternalCopy expects 1 parameter");
			}
			shared_ptr<ExternalCopy> value = shared_ptr<ExternalCopy>(ExternalCopy::Copy(args[0]));
			if (value.get() == nullptr) {
				// v8 exception should be set
				return;
			}
			Wrap(std::make_unique<ExternalCopyHandle>(value), args.This());
			args.GetReturnValue().Set(args.This());
		}

		void Copy(const FunctionCallbackInfo<Value>& args) {
			args.GetReturnValue().Set(value->CopyInto());
		}

		void CopyInto(const FunctionCallbackInfo<Value>& args) {
			args.GetReturnValue().Set(ClassHandle::NewInstance<ExternalCopyIntoHandle>(value));
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
			return Inherit<TransferableHandle>(MakeClass("ExternalCopyInto", New, 0));
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor ExternalCopyInto is private");
		}

		unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ExternalCopyIntoTransferable>(value);
		}

};

}
