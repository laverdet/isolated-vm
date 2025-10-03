#include "session_handle.h"
#include "isolate/remote_handle.h"
#include "isolate/util.h"

using namespace v8;
using namespace v8_inspector;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * This class handles sending messages from the backend to the frontend
 */
class SessionImpl : public InspectorSession {
	public:
		shared_ptr<IsolateHolder> isolate; // This is the isolate that owns the session
		RemoteHandle<Object> handle; // Reference to the JS SessionHandle instance

		explicit SessionImpl(IsolateEnvironment& isolate) : InspectorSession(isolate) {}

	private:
		// Helper
		static auto bufferToString(StringBuffer& buffer) -> MaybeLocal<String> {
			const StringView& view = buffer.string();
			if (view.is8Bit()) {
				return String::NewFromOneByte(Isolate::GetCurrent(), view.characters8(), NewStringType::kNormal, view.length());
			} else {
				return String::NewFromTwoByte(Isolate::GetCurrent(), view.characters16(), NewStringType::kNormal, view.length());
			}
		}

		// These functions are invoked directly from v8
		void sendResponse(int call_id, unique_ptr<StringBuffer> message) final {
			if (!handle) {
				return;
			}
			struct SendResponseTask : public Runnable {
				int call_id;
				unique_ptr<StringBuffer> message;
				RemoteHandle<Object> handle;

				SendResponseTask(
					int call_id, unique_ptr<StringBuffer> message, RemoteHandle<Object> handle
				) :	call_id(call_id), message(std::move(message)), handle(std::move(handle)) {}

				void Run() final {
					Isolate* isolate = Isolate::GetCurrent();
					TryCatch try_catch(isolate);
					Local<Context> context = isolate->GetCurrentContext();
					Local<Object> obj = Deref(handle);
					
					// Get the onResponse callback from the JS object
					Local<Value> callback_value;
					if (!obj->Get(context, v8_string("onResponse")).ToLocal(&callback_value) || !callback_value->IsFunction()) {
						return;
					}
					
					Local<Function> fn = callback_value.As<Function>();
					Local<String> string;
					if (bufferToString(*message).ToLocal(&string)) {
						Local<Value> argv[2];
						argv[0] = Integer::New(isolate, call_id);
						argv[1] = string;
						try {
							Unmaybe(fn->Call(context, Undefined(isolate), 2, argv));
						} catch (const RuntimeError& err) {}
					}
				}
			};
			isolate->ScheduleTask(std::make_unique<SendResponseTask>(call_id, std::move(message), handle), false, true);
		}

		void sendNotification(unique_ptr<StringBuffer> message) final {
			if (!handle) {
				return;
			}
			struct SendNotificationTask : public Runnable {
				unique_ptr<StringBuffer> message;
				RemoteHandle<Object> handle;
				SendNotificationTask(
					unique_ptr<StringBuffer> message, RemoteHandle<Object> handle
				) : message(std::move(message)), handle(std::move(handle)) {}

				void Run() final {
					Isolate* isolate = Isolate::GetCurrent();
					TryCatch try_catch(isolate);
					Local<Context> context = isolate->GetCurrentContext();
					Local<Object> obj = Deref(handle);
					
					// Get the onNotification callback from the JS object
					Local<Value> callback_value;
					if (!obj->Get(context, v8_string("onNotification")).ToLocal(&callback_value) || !callback_value->IsFunction()) {
						return;
					}
					
					Local<Function> fn = callback_value.As<Function>();
					Local<String> string;
					if (bufferToString(*message).ToLocal(&string)) {
						Local<Value> argv[1];
						argv[0] = string;
						try {
							Unmaybe(fn->Call(context, Undefined(isolate), 1, argv));
						} catch (const RuntimeError& err) {}
					}
				}
			};
			isolate->ScheduleTask(std::make_unique<SendNotificationTask>(std::move(message), handle), false, true);
		}

		void flushProtocolNotifications() final {}
};

/**
 * SessionHandle implementation
 */
SessionHandle::SessionHandle(IsolateEnvironment& isolate) : session(std::make_shared<SessionImpl>(isolate)) {
	session->isolate = IsolateEnvironment::GetCurrentHolder();
}

auto SessionHandle::Definition() -> Local<FunctionTemplate> {
	return MakeClass(
		"Session", nullptr,
		"dispatchProtocolMessage", MemberFunction<decltype(&SessionHandle::DispatchProtocolMessage), &SessionHandle::DispatchProtocolMessage>{},
		"dispose", MemberFunction<decltype(&SessionHandle::Dispose), &SessionHandle::Dispose>{}
	);
}

void SessionHandle::SetJSHandle(Local<Object> js_handle) {
	session->handle = RemoteHandle<Object>(js_handle);
}

void SessionHandle::CheckDisposed() {
	if (!session) {
		throw RuntimeGenericError("Session is dead");
	}
}

/**
 * JS API methods
 */
auto SessionHandle::DispatchProtocolMessage(Local<String> message) -> Local<Value> {
	Isolate* isolate = Isolate::GetCurrent();
	CheckDisposed();
	String::Value v8_str{isolate, message};
	session->DispatchBackendProtocolMessage(std::vector<uint16_t>(*v8_str, *v8_str + v8_str.length()));
	return Undefined(isolate);
}

auto SessionHandle::Dispose() -> Local<Value> {
	CheckDisposed();
	session.reset();
	return Undefined(Isolate::GetCurrent());
}

} // namespace ivm
