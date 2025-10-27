module;
#include <concepts>
export module napi_js:environment_fwd;

namespace js::napi {

// A reference to an environment is used as the lock witness. Generally, you should not have an
// `environment&` unless you're in the napi thread and locked.
export class environment;

// Environment constraint
export template <class Type>
concept auto_environment = std::derived_from<Type, environment>;

} // namespace js::napi
