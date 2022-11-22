#include <v8.h>
#include <v8-profiler.h>
#include "isolate/class_handle.h"

using namespace v8;

namespace ivm {

class CpuProfilerHandle : public ClassHandle {
    private:
        v8::CpuProfiler* profiler;
        void FlatNodes(const v8::CpuProfileNode* node, std::vector<const v8::CpuProfileNode*>* nodes);
        static auto CreateNode(Isolate* isolate, const v8::CpuProfileNode* node) -> Local<Object>;
        static auto CreateCallFrame(Isolate* isolate, const v8::CpuProfileNode* node) -> Local<Object>;
    public:
        void StartProfiling(Local<String> title, Local<Boolean> recordSamples);
        template <int async> auto StopProfiling(Local<String> title) -> Local<Value>;
        void SetSamplingInterval(Local<Number> microSecond);
        void SetUsePreciseSampling(Local<Boolean> isOn);
        void Dispose();
        explicit CpuProfilerHandle(v8::CpuProfiler* profiler);
        static auto Definition() -> Local<FunctionTemplate>;
};

}  // namespace ivm