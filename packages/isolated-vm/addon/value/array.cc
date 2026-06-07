module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.array;
import :handle.value_of;
import :value.record;
import auto_js;

namespace isolated_vm {
using namespace js;

// value_of<list_tag>
class EXPORT value_for_array : public value_next<list_tag> {
	public:
		using value_next<list_tag>::value_next;
		auto set(const runtime_lock& lock, int key, value_of<> value) const -> void;
		static auto make(const runtime_lock& lock, int capacity = 0) -> value_of<list_tag>;
};

} // namespace isolated_vm
