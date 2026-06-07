module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.prototype;
import :handle.value_of;
import :support.callback_storage;

namespace isolated_vm {
using namespace js;

// value_of<prototype_tag>
class EXPORT value_for_prototype : public value_next<prototype_tag> {
	public:
		using value_next<prototype_tag>::value_next;
		[[nodiscard]] static auto make(const basic_lock& lock, runtime_callback_data_allocated_type data, int length) -> value_of<function_prototype_tag>;
		[[nodiscard]] static auto make(const basic_lock& lock, runtime_callback_data_span_type data, int length) -> value_of<function_prototype_tag>;

		template <class Type>
		[[nodiscard]] static auto make(const basic_lock& lock, runtime_callback_function_storage<Type> data, int length) -> value_of<function_prototype_tag> {
			auto data_span = std::span{reinterpret_cast<std::byte*>(&data), sizeof(data)};
			return make(lock, data_span, length);
		}
};

} // namespace isolated_vm
