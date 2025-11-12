module;
#ifndef V8_VIA_NODEJS
#include "libplatform/libplatform.h"
#endif
export module v8:libplatform;

// NOLINTBEGIN(misc-unused-using-decls)
#ifndef V8_VIA_NODEJS
namespace v8::platform {
export using platform::NewDefaultJobHandle;
export using platform::NewDefaultPlatform;
} // namespace v8::platform
#endif
// NOLINTEND(misc-unused-using-decls)
