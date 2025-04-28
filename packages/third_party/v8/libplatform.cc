// NOLINTBEGIN(misc-unused-using-decls)
module;
#ifndef IVM_V8_VIA_NODEJS
#include "libplatform/libplatform.h"
#endif
export module v8:libplatform;

#ifndef IVM_V8_VIA_NODEJS
namespace v8::platform {
export using platform::NewDefaultPlatform;
} // namespace v8::platform
#endif
// NOLINTEND(misc-unused-using-decls)
