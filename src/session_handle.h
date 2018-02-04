#pragma once
#include <v8.h>
#include <v8-inspector.h>
#include "isolate/class_handle.h"
#include "isolate/inspector.h"
#include <memory>

namespace ivm {

/**
 * This handle is given to the client and encapsulates an inspector session. Non-transferable.
 */
class SessionHandle : public ClassHandle {
	private:
		std::shared_ptr<class SessionImpl> session;

	public:
		explicit SessionHandle(IsolateEnvironment& isolate);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();

		void CheckDisposed();
		v8::Local<v8::Value> DispatchProtocolMessage(v8::Local<v8::String> message);
		v8::Local<v8::Value> Dispose();
		v8::Local<v8::Value> OnNotificationGetter();
		void OnNotificationSetter(v8::Local<v8::Function> value);
		v8::Local<v8::Value> OnResponseGetter();
		void OnResponseSetter(v8::Local<v8::Function> value);
};

}
