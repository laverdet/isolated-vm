export module isolated_js.struct_.types;
import ivm.utility;

namespace js {

// You override this with an object property descriptor for each accepted type
export template <class Type>
struct struct_properties {};

} // namespace js
