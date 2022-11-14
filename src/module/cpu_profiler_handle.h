#include <v8.h>
#include <v8-profiler.h>
#include "isolate/class_handle.h"

using namespace v8;
using v8::CpuProfiler;

namespace ivm {

class CpuProfilerHandle : public ClassHandle {
    private:
        v8::CpuProfiler* profiler;
        void FlatNodes(const v8::CpuProfileNode* node, std::vector<const v8::CpuProfileNode*>* nodes);
        auto CreateNode(Isolate* isolate, const v8::CpuProfileNode* node) -> Local<Object>;
        auto CreateCallFrame(Isolate* isolate, const v8::CpuProfileNode* node) -> Local<Object>;
    public:
        void StartProfiling(Local<String> title, Local<Boolean> recordSamples);
        template <int async> auto StopProfiling(Local<String> title) -> Local<Value>;
        void SetSamplingInterval(Local<Number> us);
        void SetUsePreciseSampling(Local<Boolean> isOn);
        explicit CpuProfilerHandle(v8::CpuProfiler* profiler);
        static auto Definition() -> Local<FunctionTemplate>;
};

}  // namespace ivm