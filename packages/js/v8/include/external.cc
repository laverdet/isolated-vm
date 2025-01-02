module;
#include <type_traits>
#include <utility>
export module ivm.iv8:external;
import ivm.js;
import ivm.utility;
import v8;

namespace ivm::js::iv8 {

export class external : public v8::External {
	public:
		[[nodiscard]] auto materialize(std::type_identity<void*> /*tag*/) const -> void*;
		static auto Cast(v8::Value* value) -> external*;
};

auto external::Cast(v8::Value* value) -> external* {
	return reinterpret_cast<external*>(v8::External::Cast(value));
}

auto external::materialize(std::type_identity<void*> /*tag*/) const -> void* {
	return Value();
}

export template <class Type>
class external_reference : util::non_moveable {
	public:
		explicit external_reference(auto&&... args) :
				value{std::forward<decltype(args)>(args)...} {}

		auto operator*() -> Type& { return value; }
		auto operator->() -> Type* { return &value; }

	private:
		Type value;
};

} // namespace ivm::js::iv8

namespace ivm::js {

template <class Type>
struct accept<void, iv8::external_reference<Type>> : accept<void, void> {
		using accept<void, void>::accept;
		auto operator()(external_tag /*tag*/, auto value) const -> iv8::external_reference<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<iv8::external_reference<Type>*>(void_ptr);
		}
};

} // namespace ivm::js
