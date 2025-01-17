module;
#include <type_traits>
export module v8_js.boolean;
import v8_js.handle;
import v8;

namespace js::iv8 {

export class boolean
		: public v8::Local<v8::Boolean>,
			public handle_materializable<boolean> {
	public:
		explicit boolean(v8::Local<v8::Boolean> handle) :
				Local{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<bool> /*tag*/) const -> bool;
};

// ---

auto boolean::materialize(std::type_identity<bool> /*tag*/) const -> bool {
	return (*this)->Value();
}

} // namespace js::iv8
