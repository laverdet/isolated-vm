module;
#include <cstdint>
#include <string>
#include <variant>
export module ivm.value:primitive.types;
import :date;
import :tag;

namespace ivm::js {

export using number_t = std::variant<int32_t, uint32_t, double>;
export using bigint_t = std::variant<int64_t, uint64_t>;
export using string_t = std::variant<std::string, std::u16string>;

} // namespace ivm::js
