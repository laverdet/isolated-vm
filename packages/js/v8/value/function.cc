module v8_js;
import :function;
import :unmaybe;

namespace js::iv8 {

function_template::operator v8::Local<v8::Function>() const {
	return unmaybe((*this)->GetFunction(this->context()));
}

} // namespace js::iv8
