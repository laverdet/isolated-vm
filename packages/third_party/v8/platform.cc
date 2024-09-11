module;
#include <v8-platform.h>
#ifndef IVM_V8_VIA_NODEJS
#include <libplatform/v8-tracing.h>
#endif
#include <v8-source-location.h>
export module v8:platform;
namespace v8 {
export using v8::IdleTask;
export using v8::JobDelegate;
export using v8::JobHandle;
export using v8::JobTask;
export using v8::PageAllocator;
export using v8::Platform;
export using v8::Task;
export using v8::TaskPriority;
export using v8::TaskRunner;
#ifndef IVM_V8_VIA_NODEJS
export using v8::TracingController;
#endif
export using v8::SourceLocation;
#ifndef IVM_V8_VIA_NODEJS
namespace platform::tracing {
export using v8::platform::tracing::TracingController;
}
#endif
} // namespace v8
