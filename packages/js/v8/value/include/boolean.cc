export module v8_js:boolean;
import :handle;
import isolated_js;
import v8;

namespace js::iv8 {

export class boolean : public v8::Local<v8::Boolean> {
	public:
		explicit boolean(v8::Local<v8::Boolean> handle) :
				v8::Local<v8::Boolean>{handle} {}

		[[nodiscard]] explicit operator bool() const;
};

// ---

boolean::operator bool() const {
	return (*this)->Value();
}

} // namespace js::iv8
