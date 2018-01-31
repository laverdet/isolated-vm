#pragma once
#include <v8.h>
#include <v8-inspector.h>
#include "environment.h"

#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace ivm {
class IsolateEnvironment;

class InspectorNotifyListener {
	public:
		virtual void Notify() = 0;
};

class InspectorSession {
	private:
		unique_ptr<V8InspectorSession> session;
		shared_ptr<V8Inspector::Channel> channel;
		IsolateEnvironment* isolate;
		InspectorNotifyListener* listener;
	public:
		InspectorSession(
			IsolateEnvironment* isolate,
			V8Inspector* inspector,
			shared_ptr<V8Inspector::Channel> channel,
			InspectorNotifyListener* listener
		) : channel(channel), isolate(isolate), listener(listener) {
			session = inspector->connect(1, this->channel.get(), StringView());
		}

		void dispatchBackendProtocolMessage(std::vector<uint16_t> message);
};

class InspectorClientImpl : public V8InspectorClient, public InspectorNotifyListener {
	public:
		unique_ptr<V8Inspector> inspector;
		IsolateEnvironment* isolate;

		virtual void Notify() {
			cv.notify_all();
		}

	private:
		std::condition_variable cv;
		std::mutex mutex;
		bool running;

		void runMessageLoopOnPause(int context_group_id);

		void quitMessageLoopOnPause() {
			// This is called from within runMessageLoopOnPause, therefore lock is already acquired
			running = false;
			cv.notify_all();
		}

	public:
		InspectorClientImpl(Isolate* v8_isolate, IsolateEnvironment* isolate) : isolate(isolate) {
			inspector = V8Inspector::create(v8_isolate, this);
		}

		unique_ptr<InspectorSession> createSession(shared_ptr<V8Inspector::Channel> channel) {
			return std::make_unique<InspectorSession>(isolate, inspector.get(), channel, this);
		}
};

}
