module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.primitive;
import :handle.bound_value;
import :handle.value_of;
import :support.environment_fwd;
import auto_js;
import std;

namespace isolated_vm {
using namespace js;

// boolean
class EXPORT bound_value_for_boolean : public bound_value_next<boolean_tag> {
	public:
		using bound_value_next<boolean_tag>::bound_value_next;
		[[nodiscard]] explicit operator bool() const;
};

// number
class EXPORT bound_value_for_number : public bound_value_next<number_tag> {
	public:
		using bound_value_next<number_tag>::bound_value_next;
		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator std::int32_t() const;
		[[nodiscard]] explicit operator std::uint32_t() const;
};

// string
class EXPORT bound_value_for_string : public bound_value_next<string_tag> {
	public:
		using bound_value_next<string_tag>::bound_value_next;
		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

} // namespace isolated_vm
