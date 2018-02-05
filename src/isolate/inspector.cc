#include "inspector.h"
#include "environment.h"
#include "runnable.h"
#include <string>

using namespace ivm;
using namespace v8;
using namespace v8_inspector;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * This constructor will be called from a non-locked thread.
 */
InspectorAgent::InspectorAgent(IsolateEnvironment& isolate) : isolate(isolate), inspector(V8Inspector::create(isolate, this)) {
	struct AddDefaultContext : public Runnable {
		InspectorAgent& that;
		explicit AddDefaultContext(InspectorAgent& agent) : that(agent) {}
		void Run() final {
			// We can assume that agent is still alive because otherwise this won't get called
			that.ContextCreated(that.isolate.DefaultContext(), "default");
		}
	};
	SendInterrupt(std::make_unique<AddDefaultContext>(*this));
}

/**
 * Called right before the isolate is disposed.
 */
InspectorAgent::~InspectorAgent() {
	for (auto ii = sessions.active.begin(); ii != sessions.active.end(); ) {
		InspectorSession* session = *ii;
		++ii;
		session->Disconnect();
	}
	assert(sessions.active.empty());
}

/**
 * When debugging running JS code this function is called and is expected to block and process
 * inspector messages until the inspector is done. This happens in the isolate's currently running
 * thread so we know that an ExecutorLock is up.
 */
void InspectorAgent::runMessageLoopOnPause(int /* context_group_id */) {
	std::unique_lock<std::mutex> lock(mutex);
	running = true;
	while (running) {
		isolate.InterruptEntry();
		cv.wait(lock);
	}
}

/**
 * This is the notificaiton to exit from the currently running `runMessageLoopOnPause` from another
 * thread.
 */
void InspectorAgent::quitMessageLoopOnPause() {
	// This is called from within runMessageLoopOnPause, therefore lock is already acquired
	running = false;
	cv.notify_all();
}

/**
 * Connects a V8Inspector::Channel to a V8Inspector which returns an instance of V8InspectorSession.
 */
unique_ptr<V8InspectorSession> InspectorAgent::ConnectSession(InspectorSession& session) {
	std::unique_lock<std::mutex> lock(sessions.mutex);
	sessions.active.insert(&session);
	return inspector->connect(1, &session, StringView());
}

/**
 * Called from the session when it disconnects.
 */
void InspectorAgent::SessionDisconnected(InspectorSession& session) {
	std::unique_lock<std::mutex> lock(sessions.mutex);
	assert(sessions.active.erase(&session) == 1);
}

/**
 * Request to send an interrupt to the inspected isolate
 */
void InspectorAgent::SendInterrupt(unique_ptr<class Runnable> task) {
	// Grab pointer because it's needed for WakeIsolate
	shared_ptr<IsolateEnvironment> ptr = isolate.holder->GetIsolate();
	assert(ptr);
	// Push interrupt onto queue
	IsolateEnvironment::Scheduler::Lock scheduler(isolate.scheduler);
	scheduler.PushInterrupt(std::move(task));
	// Wake up the isolate
	if (!scheduler.WakeIsolate(ptr)) { // `true` if isolate is inactive
		// Isolate is currently running
		std::unique_lock<std::mutex> lock(mutex);
		if (running) {
			// Isolate is being debugged and is in `runMessageLoopOnPause`
			cv.notify_all();
		} else {
			// Isolate is busy running JS code
			scheduler.InterruptIsolate(isolate);
		}
	}
}

/**
 * Hook to add context to agent.
 */
void InspectorAgent::ContextCreated(v8::Local<v8::Context> context, const std::string& name) {
	inspector->contextCreated(V8ContextInfo(context, 1, StringView(reinterpret_cast<const uint8_t*>(name.c_str()), name.length())));
}

/**
 * Hook to remove context from agent.
 */
void InspectorAgent::ContextDestroyed(v8::Local<v8::Context> context) {
	inspector->contextDestroyed(context);
}

/**
 * Creates a new session and connects it to a given agent.
 */
InspectorSession::InspectorSession(IsolateEnvironment& isolate) : agent(*isolate.GetInspectorAgent()), session(agent.ConnectSession(*this)) {}

InspectorSession::~InspectorSession() {
	Disconnect();
}

/**
 * Disconnects this session from the v8 inspector. Allows inspector to deallocate gracefully.
 */
void InspectorSession::Disconnect() {
	std::unique_lock<std::mutex> lock(mutex);
	if (session) {
		agent.SessionDisconnected(*this);
		struct DeleteSession : public Runnable {
			shared_ptr<V8InspectorSession> session;
			explicit DeleteSession(shared_ptr<V8InspectorSession> session) : session(std::move(session)) {}
			// Nothing is needed since the dtor will (should) delete the session in the right context
			void Run() final {}
		};
		agent.SendInterrupt(std::make_unique<DeleteSession>(std::move(session)));
	}
}

/**
 * Send a message from this session to the backend.
 */
void InspectorSession::DispatchBackendProtocolMessage(std::vector<uint16_t> message) {
	struct DispatchMessage : public Runnable {
		std::vector<uint16_t> message;
		std::weak_ptr<V8InspectorSession> weak_session;
		explicit DispatchMessage(std::vector<uint16_t> message, shared_ptr<V8InspectorSession>& session) : message(std::move(message)), weak_session(session) {}
		void Run() final {
			std::unique_lock<std::mutex> lock;
			auto session = weak_session.lock();
			if (session) {
				session->dispatchProtocolMessage(StringView(&message[0], message.size()));
			}
		}
	};
	agent.SendInterrupt(std::make_unique<DispatchMessage>(std::move(message), session));
}

} // namespace ivm
