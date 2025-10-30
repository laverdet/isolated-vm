module;
#include <utility>
export module v8_js:external;
import :handle;
import isolated_js;
import ivm.utility;
import v8;

namespace js::iv8 {

export class external : public v8::Local<v8::External> {
	public:
		explicit external(v8::Local<v8::External> handle) :
				v8::Local<v8::External>{handle} {}

		[[nodiscard]] explicit operator void*() const;
};

// ---

external::operator void*() const {
	return (*this)->Value();
}

} // namespace js::iv8
