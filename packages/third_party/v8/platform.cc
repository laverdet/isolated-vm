module;
#include <v8-platform.h>
#include <v8-source-location.h>
export module v8:platform;
namespace v8 {
export using v8::IdleTask;
export using v8::JobDelegate;
export using v8::JobHandle;
export using v8::JobTask;
export using v8::PageAllocator;
export using v8::Platform;
export using v8::SourceLocation;
export using v8::Task;
export using v8::TaskPriority;
export using v8::TaskRunner;
export using v8::TracingController;
} // namespace v8
