module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.function;
import :handle.value_of;
import :support.lock_fwd;
import :value.primitive;

namespace isolated_vm {
using namespace js;

// value_of<function_tag>
class EXPORT value_for_function : public value_next<function_tag> {
	public:
		using value_next<function_tag>::value_next;

		template <class Result = std::monostate>
		auto apply(const runtime_lock& lock, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(const runtime_lock& lock, auto&&... args) -> Result;

	private:
		auto invoke(const runtime_lock& lock, value_of<> that, std::span<value_of<>> argv) -> value_of<>;
};

} // namespace isolated_vm
