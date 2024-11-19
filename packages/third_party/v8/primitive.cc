// NOLINTBEGIN(misc-unused-using-decls)
module;
#include "version.h"
#include <v8-primitive.h>
export module v8:primitive;
namespace v8 {
export using v8::BigInt;
export using v8::Boolean;
export using v8::Int32;
export using v8::Integer;
export using v8::Name;
export using v8::NewStringType;
export using v8::Number;
#if V8_HAS_NUMERIC
export using v8::Numeric;
#endif
export using v8::Null;
export using v8::Primitive;
export using v8::String;
export using v8::Symbol;
export using v8::Uint32;
export using v8::Undefined;
} // namespace v8
// NOLINTEND(misc-unused-using-decls)
