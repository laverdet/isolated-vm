module;
#include <napi.h>
export module napi;
export using napi_env = napi_env;
export using napi_status = napi_status;
export using napi_value = napi_value;
namespace Napi {
export using Napi::Array;
export using Napi::Boolean;
export using Napi::CallbackInfo;
export using Napi::Date;
export using Napi::Env;
export using Napi::Error;
export using Napi::Function;
export using Napi::Number;
export using Napi::Object;
export using Napi::ObjectReference;
export using Napi::Promise;
export using Napi::Reference;
export using Napi::String;
export using Napi::TypedThreadSafeFunction;
export using Napi::Value;
export using Napi::Weak;
} // namespace Napi
