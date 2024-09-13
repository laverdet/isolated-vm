module;
#include <type_traits>
export module ivm.v8:boolean;
import v8;

namespace ivm::iv8 {

export class boolean : public v8::Boolean {
	public:
		auto materialize(std::type_identity<bool> /*tag*/) const -> bool;
		static auto Cast(v8::Data* data) -> boolean*;
};

auto boolean::Cast(v8::Data* data) -> boolean* {
	return reinterpret_cast<boolean*>(v8::Boolean::Cast(data));
}

auto boolean::materialize(std::type_identity<bool> /*tag*/) const -> bool {
	return Value();
}

} // namespace ivm::iv8
