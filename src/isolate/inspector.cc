#include "environment.h"
#include "inspector.h"

using namespace ivm;
using namespace v8;
using namespace v8_inspector;

void InspectorSession::dispatchBackendProtocolMessage(std::vector<uint16_t> message) {
	auto message_ptr = std::make_shared<decltype(message)>(std::move(message));
	isolate->ScheduleInterrupt([message_ptr, this]() {
		if (session) {
			session->dispatchProtocolMessage(v8_inspector::StringView(&(*message_ptr)[0], message_ptr->size()));
		}
	});
	listener->Notify();
}


void InspectorClientImpl::runMessageLoopOnPause(int context_group_id) {
	std::unique_lock<std::mutex> lock(mutex);
	running = true;
	while (running) {
		IsolateEnvironment::InterruptEntry(Isolate::GetCurrent(), isolate);
		cv.wait(lock);
	}
}

