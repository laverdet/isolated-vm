#include "cpu_profiler_handle.h"

#include <exception>
#include <typeinfo>
#include <stdexcept>
#include <cstring>

using namespace v8;

namespace ivm {

auto CpuProfilerHandle::Definition() -> Local<FunctionTemplate> {
    return MakeClass(
		"CpuProfiler", nullptr,
        "startProfiling", MemberFunction<decltype(&CpuProfilerHandle::StartProfiling), &CpuProfilerHandle::StartProfiling>{},
		"stopProfiling", MemberFunction<decltype(&CpuProfilerHandle::StopProfiling<1>), &CpuProfilerHandle::StopProfiling<1>>{},
        "setSamplingInterval", MemberFunction<decltype(&CpuProfilerHandle::SetSamplingInterval), &CpuProfilerHandle::SetSamplingInterval>{},
        "setUsePreciseSampling", MemberFunction<decltype(&CpuProfilerHandle::SetUsePreciseSampling), &CpuProfilerHandle::SetUsePreciseSampling>{}
	);
}

CpuProfilerHandle::CpuProfilerHandle(v8::CpuProfiler* profiler) : profiler(profiler) {} 

void CpuProfilerHandle::FlatNodes(const v8::CpuProfileNode* node, std::vector<const v8::CpuProfileNode*>* nodes) {
    nodes->push_back(node);
    uint32_t childrenCount = node->GetChildrenCount();

    for (uint32_t index = 0; index < childrenCount; ++index) {
        FlatNodes(node->GetChild(index), nodes);
    }
}

void CpuProfilerHandle::StartProfiling(Local<String> title, Local<Boolean> recordSamples) {
    if (!profiler) {
        throw RuntimeGenericError("`profiler` is disposed");
    }

    profiler->StartProfiling(title, recordSamples->Value());
}

template <int async> 
auto CpuProfilerHandle::StopProfiling(v8::Local<v8::String> title) -> Local<Value> {
    if (!profiler) {
        throw RuntimeGenericError("`profiler` is disposed");
    }
    v8::CpuProfile* profile;

    profile = profiler->StopProfiling(title);

    Isolate* iso = Isolate::GetCurrent();
    Local<Context> context = iso->GetCurrentContext();

    v8::EscapableHandleScope handle_scope(iso);
    Local<Object> profileObject = Object::New(iso);
    auto& strings = StringTable::Get();

    Unmaybe(profileObject->Set(context, strings.startTime, Number::New(iso, profile->GetStartTime())));
    Unmaybe(profileObject->Set(context, strings.endTime, Number::New(iso, profile->GetEndTime())));
    Unmaybe(profileObject->Set(context, strings.title, title));

    uint64_t sampleCount = profile->GetSamplesCount();
    Local<Array> samples = Array::New(iso, sampleCount);
    Local<Array> timeDeltas = Array::New(iso, sampleCount);

    uint64_t startTs = profile->GetStartTime();    
    for (uint64_t i = 0; i < sampleCount; i++) {
        Unmaybe(samples->Set(context, i, Number::New(iso, profile->GetSample(i)->GetNodeId())));
        Unmaybe(timeDeltas->Set(context, i, Number::New(iso, profile->GetSampleTimestamp(i) - startTs)));
    }
    Unmaybe(profileObject->Set(context, strings.samples, samples));
    Unmaybe(profileObject->Set(context, strings.timeDeltas, timeDeltas));

    // handle nodes
    std::vector<const v8::CpuProfileNode*> nodes;
    FlatNodes(profile->GetTopDownRoot(), &nodes);
    
    size_t count = nodes.size();
    Local<Array> nodeArr = Array::New(iso, count);
    Unmaybe(profileObject->Set(context, strings.nodes, nodeArr));

    for (size_t i = 0; i < count; i++) {
        Local<Object> nodeObj = CreateNode(iso, nodes.at(i));
        Unmaybe(nodeArr->Set(context, i, nodeObj));
    }

    profiler->Dispose();
    profiler = NULL;

    return handle_scope.Escape(profileObject);
}

void CpuProfilerHandle::SetSamplingInterval(Local<Number> microSecond) {
    profiler->SetSamplingInterval((int)microSecond->Value());
}

void CpuProfilerHandle::SetUsePreciseSampling(Local<Boolean> isOn) {
    // nodejs @ v10 does not have this method, will simply ignore
    #if NODE_MODULE_VERSION >= 72
        profiler->SetUsePreciseSampling(isOn->Value());
    #endif
}

auto CpuProfilerHandle::CreateNode(Isolate *isolate, const v8::CpuProfileNode* node) -> Local<Object> {
    auto& strings = StringTable::Get();
    Local<Object> nodeObj = Object::New(isolate);
    Local<Context> context = isolate->GetCurrentContext();      

    Unmaybe(nodeObj->Set(context, strings.hitCount, Number::New(isolate, node->GetHitCount())));
    Unmaybe(nodeObj->Set(context, strings.id, Number::New(isolate, node->GetNodeId())));

    const char* bailoutReason = node->GetBailoutReason();

    if (strlen(bailoutReason) > 0) {
        #if NODE_MODULE_VERSION < 83
            Unmaybe(nodeObj->Set(context, strings.bailoutReason, String::NewFromUtf8(isolate, bailoutReason)));
        #else
            Unmaybe(nodeObj->Set(context, strings.bailoutReason, String::NewFromUtf8(isolate, bailoutReason).ToLocalChecked()));
        #endif
    }
    
    size_t childrenCount = node->GetChildrenCount();
    Local<Array> children = Array::New(isolate, childrenCount);
    Unmaybe(nodeObj->Set(context, strings.children, children));

    for (size_t j = 0; j < childrenCount; j++) {
        Unmaybe(children->Set(context, j, Number::New(isolate, node->GetChild(j)->GetNodeId())));
    }

    Local<Object> callFrame = CreateCallFrame(isolate, node);
    Unmaybe(nodeObj->Set(context, strings.callFrame, callFrame));
    

    return nodeObj;
}

auto CpuProfilerHandle::CreateCallFrame(Isolate *isolate, const v8::CpuProfileNode* node) -> Local<Object> {
    auto& strings = StringTable::Get();
    Local<Object> callFrame = Object::New(isolate);
    Local<Context> context = isolate->GetCurrentContext();  
    #if NODE_MODULE_VERSION < 83
        Unmaybe(callFrame->Set(context, strings.functionName, String::NewFromUtf8(isolate, node->GetFunctionNameStr())));
        Unmaybe(callFrame->Set(context, strings.url, String::NewFromUtf8(isolate, node->GetScriptResourceNameStr())));
    #else
        Unmaybe(callFrame->Set(context, strings.functionName, String::NewFromUtf8(isolate, node->GetFunctionNameStr()).ToLocalChecked()));
        Unmaybe(callFrame->Set(context, strings.url, String::NewFromUtf8(isolate, node->GetScriptResourceNameStr()).ToLocalChecked()));
    #endif
    Unmaybe(callFrame->Set(context, strings.scriptId, Number::New(isolate, node->GetScriptId())));
    Unmaybe(callFrame->Set(context, strings.lineNumber, Number::New(isolate, node->GetLineNumber())));
    Unmaybe(callFrame->Set(context, strings.columnNumber, Number::New(isolate, node->GetColumnNumber())));
    return callFrame;
}

} // end namespace ivm