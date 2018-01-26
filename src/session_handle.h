#pragma once
#include <node.h>
#include <v8-inspector.h>

#include "class_handle.h"
#include "inspector.h"

#include <memory>

namespace ivm {
using namespace v8;
using namespace v8_inspector;
using std::unique_ptr;

/**
 * This class handles sending messages from the backend to the frontend
 */
class ChannelImpl : public V8Inspector::Channel {
	public:
		shared_ptr<ShareablePersistent<Function>> onError;
		shared_ptr<ShareablePersistent<Function>> onNotification;
		shared_ptr<ShareablePersistent<Function>> onResponse;

	private:
		static MaybeLocal<String> bufferToString(shared_ptr<StringBuffer> buffer) {
			const StringView& view = buffer->string();
			if (view.is8Bit()) {
				return String::NewFromOneByte(Isolate::GetCurrent(), view.characters8(), v8::NewStringType::kNormal, view.length());
			} else {
				return String::NewFromTwoByte(Isolate::GetCurrent(), view.characters16(), v8::NewStringType::kNormal, view.length());
			}
		}

		void sendResponse(int call_id, unique_ptr<StringBuffer> message) {
			try {
				auto onResponse_ptr = onResponse;
				if (!onResponse_ptr) return;
				shared_ptr<StringBuffer> message_ptr = std::move(message);
				onResponse_ptr->GetIsolate().ScheduleTask([call_id, message_ptr, onResponse_ptr]() {
					Local<String> string;
					if (bufferToString(message_ptr).ToLocal(&string)) {
						Local<Function> fn = onResponse_ptr->Deref();
						Local<Value> argv[2];
						Isolate* isolate = Isolate::GetCurrent();
						argv[0] = Integer::New(isolate, call_id);
						argv[1] = string;
						try {
							Unmaybe(fn->Call(isolate->GetCurrentContext(), Undefined(isolate), 2, argv));
						} catch (const js_error_base& err) {}
					}
				});
			} catch (const js_error_base& err) {}
		}

		void sendNotification(unique_ptr<StringBuffer> message) {
			try {
				auto onNotification_ptr = onNotification;
				if (!onNotification_ptr) return;
				shared_ptr<StringBuffer> message_ptr = std::move(message);
				onNotification_ptr->GetIsolate().ScheduleTask([message_ptr, onNotification_ptr]() {
					Local<String> string;
					if (bufferToString(message_ptr).ToLocal(&string)) {
						Local<Function> fn = onNotification_ptr->Deref();
						Local<Value> argv[1];
						Isolate* isolate = Isolate::GetCurrent();
						argv[0] = string;
						try {
							Unmaybe(fn->Call(isolate->GetCurrentContext(), Undefined(isolate), 1, argv));
						} catch (const js_error_base& err) {}
					}
				});
			} catch (const js_error_base& err) {}
		}

		void flushProtocolNotifications() {}
};

/**
 * This handle is given to the client and encapsulates an inspector session.
 */
class SessionHandle : public ClassHandle {
	private:
		shared_ptr<ChannelImpl> channel;
		unique_ptr<InspectorSession> session;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			Local<FunctionTemplate> tmpl = Inherit<TransferableHandle>(MakeClass(
				"Session", nullptr, 0,
				"dispatchProtocolMessage", Parameterize<decltype(&SessionHandle::DispatchProtocolMessage), &SessionHandle::DispatchProtocolMessage>, 1,
				"dispose", Parameterize<decltype(&SessionHandle::Dispose), &SessionHandle::Dispose>, 0
			));
			tmpl->InstanceTemplate()->SetAccessor(v8_symbol("onNotification"), OnNotificationGetter, OnNotificationSetter);
			tmpl->InstanceTemplate()->SetAccessor(v8_symbol("onResponse"), OnResponseGetter, OnResponseSetter);
			return tmpl;
		}

		SessionHandle(ShareableIsolate* isolate) :
			channel(std::make_shared<ChannelImpl>()),
			session(isolate->CreateInspectorSession(channel)) {}

		Local<Value> DispatchProtocolMessage(Local<Value> message) {
			if (!session) {
				throw js_generic_error("Session is dead");
			} else if (message->IsString()) {
				String::Value v8_str(Local<String>::Cast(message));
				session->dispatchBackendProtocolMessage(std::vector<uint16_t>(*v8_str, *v8_str + v8_str.length()));
				return Undefined(Isolate::GetCurrent());
			} else {
				throw js_type_error("`message` must be string or Uint8Array");
			}
		}

		Local<Value> Dispose() {
			if (session) {
				session.reset();
			} else {
				throw js_generic_error("Session is dead");
			}
			return Undefined(Isolate::GetCurrent());
		}

		// .onNotification
		static void OnNotificationGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
			SessionHandle& that = *dynamic_cast<SessionHandle*>(Unwrap(info.This()));
			info.GetReturnValue().Set(that.channel->onNotification->Deref());
		}

		static void OnNotificationSetter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
			SessionHandle& that = *dynamic_cast<SessionHandle*>(Unwrap(info.This()));
			if (!value->IsFunction()) {
				Isolate::GetCurrent()->ThrowException(Exception::TypeError(v8_string("`onNotification` must be a function")));
			}
			that.channel->onNotification = std::make_shared<ShareablePersistent<Function>>(value.As<Function>());
		}

		// .onResponse
		static void OnResponseGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
			SessionHandle& that = *dynamic_cast<SessionHandle*>(Unwrap(info.This()));
			info.GetReturnValue().Set(that.channel->onResponse->Deref());
		}

		static void OnResponseSetter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
			SessionHandle& that = *dynamic_cast<SessionHandle*>(Unwrap(info.This()));
			if (!value->IsFunction()) {
				Isolate::GetCurrent()->ThrowException(Exception::TypeError(v8_string("`onResponse` must be a function")));
			}
			that.channel->onResponse = std::make_shared<ShareablePersistent<Function>>(value.As<Function>());
		}
};

}
