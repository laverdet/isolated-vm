module;
#include <type_traits>
export module ivm.v8:external;
import ivm.utility;
import ivm.value;
import v8;

namespace ivm::iv8 {

export class external : public v8::External {
	public:
		[[nodiscard]] auto materialize(std::type_identity<void*> /*tag*/) const -> void*;
		static auto Cast(v8::Value* value) -> external*;
};

auto external::Cast(v8::Value* value) -> external* {
	return reinterpret_cast<external*>(v8::External::Cast(value));
}

auto external::materialize(std::type_identity<void*> /*tag*/) const -> void* {
	return static_cast<void*>(Value());
}

} // namespace ivm::iv8
