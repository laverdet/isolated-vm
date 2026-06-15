module napi_js;
import std;

namespace js::napi {

// undefined & null
auto accept_basic_napi_value::operator()(undefined_tag /*tag*/, visit_holder /*visit*/, std::monostate /*subject*/) const -> value_of<undefined_tag> {
	return value_of<undefined_tag>::from(napi::invoke(napi_get_undefined, napi_env{*this}));
}

auto accept_basic_napi_value::operator()(null_tag /*tag*/, visit_holder /*visit*/, std::nullptr_t /*subject*/) const -> value_of<null_tag> {
	return value_of<null_tag>::from(napi::invoke(napi_get_null, napi_env{*this}));
}

// boolean
auto accept_basic_napi_value::operator()(boolean_tag /*tag*/, visit_holder /*visit*/, bool subject) const -> value_of<boolean_tag> {
	return value_of<boolean_tag>::from(napi::invoke(napi_get_boolean, napi_env{*this}, subject));
}

// number
auto accept_basic_napi_value::operator()(number_tag_of<double> /*tag*/, visit_holder /*visit*/, double subject) const -> value_of<number_tag_of<double>> {
	return value_of<number_tag_of<double>>::from(napi::invoke(napi_create_double, napi_env{*this}, subject));
}

auto accept_basic_napi_value::operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, std::int32_t subject) const -> value_of<number_tag_of<std::int32_t>> {
	return value_of<number_tag_of<std::int32_t>>::from(napi::invoke(napi_create_int32, napi_env{*this}, subject));
}

auto accept_basic_napi_value::operator()(number_tag_of<std::uint32_t> /*tag*/, visit_holder /*visit*/, std::uint32_t subject) const -> value_of<number_tag_of<std::uint32_t>> {
	return value_of<number_tag_of<std::uint32_t>>::from(napi::invoke(napi_create_uint32, napi_env{*this}, subject));
}

// string
auto accept_basic_napi_value::operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::string_view subject) const
	-> js::referenceable_value<value_of<string_tag_of<char>>> {
	auto* value = napi::invoke(napi_create_string_latin1, napi_env{*this}, subject.data(), subject.length());
	return js::referenceable_value{value_of<string_tag_of<char>>::from(value)};
}

auto accept_basic_napi_value::operator()(string_tag_of<char8_t> /*tag*/, visit_holder /*visit*/, std::u8string_view subject) const
	-> js::referenceable_value<value_of<string_tag_of<char8_t>>> {
	auto* value = napi::invoke(napi_create_string_utf8, napi_env{*this}, reinterpret_cast<const char*>(subject.data()), subject.length());
	return js::referenceable_value{value_of<string_tag_of<char8_t>>::from(value)};
}

auto accept_basic_napi_value::operator()(string_tag_of<char16_t> /*tag*/, visit_holder /*visit*/, std::u16string_view subject) const
	-> js::referenceable_value<value_of<string_tag_of<char16_t>>> {
	auto* value = napi::invoke(napi_create_string_utf16, napi_env{*this}, subject.data(), subject.length());
	return js::referenceable_value{value_of<string_tag_of<char16_t>>::from(value)};
}

// bigint
auto accept_basic_napi_value::operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, std::int64_t subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<std::int64_t>>> {
	auto* value = napi::invoke(napi_create_bigint_int64, napi_env{*this}, subject);
	return js::referenceable_value{value_of<bigint_tag_of<std::int64_t>>::from(value)};
}

auto accept_basic_napi_value::operator()(bigint_tag_of<std::uint64_t> /*tag*/, visit_holder /*visit*/, std::uint64_t subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<std::uint64_t>>> {
	auto* value = napi::invoke(napi_create_bigint_uint64, napi_env{*this}, subject);
	return js::referenceable_value{value_of<bigint_tag_of<std::uint64_t>>::from(value)};
}

auto accept_basic_napi_value::operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
	auto* value = napi::invoke(napi_create_bigint_words, napi_env{*this}, subject.sign_bit(), subject.size(), subject.data());
	return js::referenceable_value{value_of<bigint_tag_of<js::bigint>>::from(value)};
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
auto accept_basic_napi_value::operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
	-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
	return (*this)(tag, visit, static_cast<const js::bigint&>(subject));
}

// date
auto accept_basic_napi_value::operator()(date_tag /*tag*/, visit_holder /*visit*/, js_clock::time_point subject) const
	-> js::referenceable_value<value_of<date_tag>> {
	auto value = value_of<date_tag>::from(napi::invoke(napi_create_date, napi_env{*this}, subject.time_since_epoch().count()));
	return js::referenceable_value{value};
}

// error
auto accept_basic_napi_value::operator()(error_tag /*tag*/, visit_holder visit, const js::error_value& subject) const
	-> js::referenceable_value<value_of<error_tag>> {
	auto* message = napi_value{js::transfer_in<value_of<string_tag>>(subject.message(), environment())};
	auto* error = [ & ] -> napi_value {
		switch (subject.name()) {
			default:
				return napi::invoke(napi_create_error, napi_env{*this}, napi_value{}, message);
			case js::error_name::range_error:
				return napi::invoke(napi_create_range_error, napi_env{*this}, napi_value{}, message);
			case js::error_name::syntax_error:
				return napi::invoke(node_api_create_syntax_error, napi_env{*this}, napi_value{}, message);
			case js::error_name::type_error:
				return napi::invoke(napi_create_type_error, napi_env{*this}, napi_value{}, message);
		}
	}();
	auto stack = (*this)(string_tag{}, visit, subject.stack());
	napi::invoke0(napi_set_named_property, napi_env{*this}, error, "stack", *std::move(stack));
	return js::referenceable_value{value_of<error_tag>::from(error)};
}

// data blocks (array buffer, shared array buffer)
auto accept_basic_napi_value::operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, const js::array_buffer& subject) const
	-> js::referenceable_value<value_of<array_buffer_tag>> {
	auto view = std::span<const std::byte>{subject};
	// You could avoid the extra copy for `js::data_block&&` here w/ v8 API. Napi requires the copy
	// though.
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	void* bytes;
	auto* result = napi::invoke(napi_create_arraybuffer, napi_env{*this}, view.size(), &bytes);
	// nb: `std::memcpy` *technically* results in undefined behavior on block size 0
	// (and also) it maybe causes an infinite loop with musl
	// https://stackoverflow.com/questions/5243012/is-it-guaranteed-to-be-safe-to-perform-memcpy0-0-0
	std::ranges::copy(view, static_cast<std::byte*>(bytes));
	auto value = value_of<array_buffer_tag>::from(result);
	return js::referenceable_value{value};
}

auto accept_basic_napi_value::operator()(shared_array_buffer_tag /*tag*/, visit_holder /*visit*/, js::shared_array_buffer&& subject) const
	-> js::referenceable_value<value_of<shared_array_buffer_tag>> {
	auto value = make_shared_array_buffer(std::move(subject).acquire_ownership(), subject.byte_length());
	return js::referenceable_value{value};
}

auto accept_basic_napi_value::operator()(shared_array_buffer_tag tag, visit_holder visit, const js::shared_array_buffer& subject) const
	-> js::referenceable_value<value_of<shared_array_buffer_tag>> {
	return (*this)(tag, visit, js::shared_array_buffer{subject});
}

} // namespace js::napi
