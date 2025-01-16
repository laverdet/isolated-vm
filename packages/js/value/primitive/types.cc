module;
#include <cstdint>
#include <string>
#include <variant>
export module isolated_js.primitive.types;
import isolated_js.date;
import isolated_js.tag;

namespace js {

export using number_t = std::variant<int32_t, uint32_t, double>;
export using bigint_t = std::variant<int64_t, uint64_t>;
export using string_t = std::variant<std::string, std::u16string>;

} // namespace js
