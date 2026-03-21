module;
#ifdef INCLUDE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif
export module v8:libplatform;

// NOLINTBEGIN(misc-unused-using-decls)
#ifdef INCLUDE_V8_PLATFORM
namespace v8::platform {
export using platform::NewDefaultJobHandle;
export using platform::NewDefaultPlatform;
} // namespace v8::platform
#endif
// NOLINTEND(misc-unused-using-decls)
