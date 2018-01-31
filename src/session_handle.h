#pragma once
#include <v8.h>
#include <v8-inspector.h>

#include "isolate/class_handle.h"
#include "isolate/inspector.h"

#include <memory>

namespace ivm {

/**
 * This class handles sending messages from the backend to the frontend
 */
class ChannelImpl : public V8Inspector::Channel {
	public:
		shared_ptr<Persistent<Function>> onError;
		shared_ptr<Persistent<Function>> onNotification;
		shared_ptr<Persistent<Function>> onResponse;

	private:
		static MaybeLocal<String> bufferToString(StringBuffer& buffer) {
			const StringView& view = buffer.string();
			if (view.is8Bit()) {
				return String::NewFromOneByte(Isolate::GetCurrent(), view.characters8(), v8::NewStringType::kNormal, view.length());
			} else {
				return String::NewFromTwoByte(Isolate::GetCurrent(), view.characters16(), v8::NewStringType::kNormal, view.length());
			}
		}

		void sendResponse(int call_id, unique_ptr<StringBuffer> message) {
			if (!onResponse) return;
			struct SendResponseTask : public Runnable {
				int call_id;
				unique_ptr<StringBuffer> message_ptr;
				shared_ptr<Persistent<Function>> onResponse;
				SendResponseTask(
					int call_id, unique_ptr<StringBuffer> message_ptr, shared_ptr<Persistent<Function>> onResponse
				)	:
					call_id(call_id), message_ptr(std::move(message_ptr)), onResponse(std::move(onResponse)) {}

				void Run() final {
					Local<String> string;
					if (bufferToString(*message_ptr).ToLocal(&string)) {
						Local<Function> fn = onResponse->Deref();
						Local<Value> argv[2];
						Isolate* isolate = Isolate::GetCurrent();
						argv[0] = Integer::New(isolate, call_id);
						argv[1] = string;
						try {
							Unmaybe(fn->Call(isolate->GetCurrentContext(), Undefined(isolate), 2, argv));
						} catch (const js_runtime_error& err) {}
					}
				}
			};
			onResponse->GetIsolate().ScheduleTask(std::make_unique<SendResponseTask>(call_id, std::move(message), onResponse), false, true);
		}

		void sendNotification(unique_ptr<StringBuffer> message) {
			if (!onNotification) return;
			struct SendNotificationTask : public Runnable {
				unique_ptr<StringBuffer> message_ptr;
				shared_ptr<Persistent<Function>> onNotification;
				SendNotificationTask(
					unique_ptr<StringBuffer> message_ptr,
					shared_ptr<Persistent<Function>> onNotification
				) :
					message_ptr(std::move(message_ptr)), onNotification(std::move(onNotification)) {}

				void Run() final {
					Local<String> string;
					if (bufferToString(*message_ptr).ToLocal(&string)) {
						Local<Function> fn = onNotification->Deref();
						Local<Value> argv[1];
						Isolate* isolate = Isolate::GetCurrent();
						argv[0] = string;
						try {
							Unmaybe(fn->Call(isolate->GetCurrentContext(), Undefined(isolate), 1, argv));
						} catch (const js_runtime_error& err) {}
					}
				}
			};
			onNotification->GetIsolate().ScheduleTask(std::make_unique<SendNotificationTask>(std::move(message), onNotification), false, true);
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
		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
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

		SessionHandle(IsolateEnvironment* isolate) :
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
			that.channel->onNotification = std::make_shared<Persistent<Function>>(value.As<Function>());
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
			that.channel->onResponse = std::make_shared<Persistent<Function>>(value.As<Function>());
		}
};

}
