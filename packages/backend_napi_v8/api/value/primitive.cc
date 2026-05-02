module isolated_vm;
import :support.cast;
import :support.environment;
import :value;
import auto_js;

namespace isolated_vm {
using namespace js;

// boolean
bound_value_for_boolean::operator bool() const {
	return js::transfer_out<bool>(cast_in(value_of{*this}), environment().witness());
}

// number
bound_value_for_number::operator double() const {
	return js::transfer_out<double>(cast_in(value_of{*this}), environment().witness());
}

bound_value_for_number::operator std::int32_t() const {
	return js::transfer_out<std::int32_t>(cast_in(value_of{*this}), environment().witness());
}

bound_value_for_number::operator std::uint32_t() const {
	return js::transfer_out<std::uint32_t>(cast_in(value_of{*this}), environment().witness());
}

// string
bound_value_for_string::operator std::string() const {
	return js::transfer_out<std::string>(cast_in(value_of{*this}), environment().witness());
}

bound_value_for_string::operator std::u16string() const {
	return js::transfer_out<std::u16string>(cast_in(value_of{*this}), environment().witness());
}

} // namespace isolated_vm
