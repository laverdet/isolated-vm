module;
#ifndef V8_VIA_NODEJS
#include "libplatform/v8-tracing.h"
#include "v8-platform.h"
#include "v8-source-location.h"
#endif
export module v8:platform;
// NOLINTBEGIN(misc-unused-using-decls)
#ifndef V8_VIA_NODEJS
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
export using v8::TracingController;
export using v8::SourceLocation;
namespace platform::tracing {
export using v8::platform::tracing::TracingController;
}
} // namespace v8
#endif
// NOLINTEND(misc-unused-using-decls)
