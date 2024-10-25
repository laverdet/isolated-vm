module;
#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.value:struct_.types;
import ivm.utility;

namespace ivm::value {

// You override this with an object property descriptor for each accepted type
export template <class Type>
struct object_properties;

// Descriptor for object getter and/or setter
export template <util::string_literal Name, auto Getter, auto Setter, bool Required = true>
struct accessor {};

// Descriptor for object property through direct member access
export template <util::string_literal Name, auto Member, bool Required = true>
struct member {};

} // namespace ivm::value
