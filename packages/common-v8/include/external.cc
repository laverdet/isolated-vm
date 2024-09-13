module;
#include <type_traits>
#include <utility>
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

} // namespace ivm::iv8

namespace ivm::value {

template <class Meta, class Type>
struct accept<Meta, iv8::external_reference<Type>> {
		auto operator()(external_tag /*tag*/, auto value) const -> iv8::external_reference<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<iv8::external_reference<Type>*>(void_ptr);
		}
};

} // namespace ivm::value
