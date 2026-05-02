module isolated_vm;
import :support.cast;
import :support.lock;
import :transfer.accept;
import auto_js;
import v8_js;
import std;

namespace isolated_vm {
using namespace js;

// undefined
auto accept_vm_primitive::operator()(undefined_tag /*tag*/, visit_holder /*visit*/, std::monostate subject) const -> value_of<undefined_tag> {
	return cast_out(js::transfer_in<v8::Local<iv8::Undefined>>(subject, lock().witness()));
}

// null
auto accept_vm_primitive::operator()(null_tag /*tag*/, visit_holder /*visit*/, std::nullptr_t subject) const -> value_of<null_tag> {
	return cast_out(js::transfer_in<v8::Local<iv8::Null>>(subject, lock().witness()));
}

// boolean
auto accept_vm_primitive::operator()(boolean_tag /*tag*/, visit_holder /*visit*/, bool subject) const -> value_of<boolean_tag> {
	return cast_out(js::transfer_in<v8::Local<v8::Boolean>>(subject, lock().witness()));
}

// number
auto accept_vm_primitive::operator()(number_tag_of<double> /*tag*/, visit_holder /*visit*/, double subject) const -> value_of<number_tag_of<double>> {
	return cast_out(js::transfer_in<v8::Local<iv8::Double>>(subject, lock().witness()));
}

auto accept_vm_primitive::operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, std::int32_t subject) const -> value_of<number_tag_of<std::int32_t>> {
	return cast_out(js::transfer_in<v8::Local<v8::Int32>>(subject, lock().witness()));
}

auto accept_vm_primitive::operator()(number_tag_of<std::uint32_t> /*tag*/, visit_holder /*visit*/, std::uint32_t subject) const -> value_of<number_tag_of<std::uint32_t>> {
	return cast_out(js::transfer_in<v8::Local<v8::Uint32>>(subject, lock().witness()));
}

// string
auto accept_vm_primitive::operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::string_view subject) const
	-> js::referenceable_value<value_of<string_tag_of<char>>> {
	auto value = js::transfer_in<v8::Local<iv8::StringOneByte>>(subject, lock().witness());
	return js::referenceable_value{cast_out(value)};
}

auto accept_vm_primitive::operator()(string_tag_of<char8_t> /*tag*/, visit_holder /*visit*/, std::u8string_view subject) const
	-> js::referenceable_value<value_of<string_tag>> {
	auto value = js::transfer_in<v8::Local<v8::String>>(subject, lock().witness());
	return js::referenceable_value{cast_out(value)};
}

auto accept_vm_primitive::operator()(string_tag_of<char16_t> /*tag*/, visit_holder /*visit*/, std::u16string_view subject) const
	-> js::referenceable_value<value_of<string_tag_of<char16_t>>> {
	auto value = js::transfer_in<v8::Local<iv8::StringTwoByte>>(subject, lock().witness());
	return js::referenceable_value{cast_out(value)};
}

// bigint
auto accept_vm_primitive::operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, std::int64_t subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<std::int64_t>>> {
	return js::referenceable_value{cast_out(js::transfer_in<v8::Local<iv8::BigInt64>>(subject, lock().witness()))};
}

auto accept_vm_primitive::operator()(bigint_tag_of<std::uint64_t> /*tag*/, visit_holder /*visit*/, std::uint64_t subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<std::uint64_t>>> {
	return js::referenceable_value{cast_out(js::transfer_in<v8::Local<iv8::BigIntU64>>(subject, lock().witness()))};
}

auto accept_vm_primitive::operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
	auto value = cast_out(js::transfer_in<v8::Local<v8::BigInt>>(subject, lock().witness()));
	return js::referenceable_value{value_of<bigint_tag_of<js::bigint>>::from(value)};
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
auto accept_vm_primitive::operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
	return (*this)(tag, visit, static_cast<const js::bigint&>(subject));
}

} // namespace isolated_vm
