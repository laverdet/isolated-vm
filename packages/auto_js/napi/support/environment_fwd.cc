export module napi_js:environment_fwd;
import std;

namespace js::napi {

// Per-instance environment state.
export class environment;

// Environment constraint
export template <class Type>
concept auto_environment = std::derived_from<Type, environment>;

// Environment lock witness, defined in `:lock`
export class environment_lock_witness;

} // namespace js::napi
