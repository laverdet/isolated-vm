module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.object;
import :value.primitive;

namespace isolated_vm {
using namespace js;

// value_of<object_tag>
class EXPORT value_for_object : public value_next<object_tag> {
	public:
		using value_next<object_tag>::value_next;
		[[nodiscard]] auto inspect() const -> value_typeof;
};

} // namespace isolated_vm
