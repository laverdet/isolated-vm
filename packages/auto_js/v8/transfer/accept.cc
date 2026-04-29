module v8_js;
import auto_js;
import std;
import v8;

namespace js::iv8 {

// null
auto accept_primitive::operator()(null_tag /*tag*/, visit_holder /*visit*/, std::nullptr_t /*subject*/) const -> v8::Local<iv8::Null> {
	return v8::Null(isolate()).As<iv8::Null>();
}

// boolean
auto accept_primitive::operator()(boolean_tag /*tag*/, visit_holder /*visit*/, bool subject) const -> v8::Local<v8::Boolean> {
	return v8::Boolean::New(isolate_, subject);
}

// number
auto accept_primitive::operator()(number_tag_of<double> /*tag*/, visit_holder /*visit*/, double subject) const -> v8::Local<iv8::Double> {
	return v8::Number::New(isolate_, subject).As<iv8::Double>();
}

auto accept_primitive::operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, std::int32_t subject) const -> v8::Local<v8::Int32> {
	return v8::Integer::New(isolate_, subject).As<v8::Int32>();
}

auto accept_primitive::operator()(number_tag_of<std::uint32_t> /*tag*/, visit_holder /*visit*/, std::uint32_t subject) const -> v8::Local<v8::Uint32> {
	return v8::Integer::NewFromUnsigned(isolate_, subject).As<v8::Uint32>();
}

// string
auto accept_primitive::operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::basic_string_view<char> subject) const
	-> js::referenceable_value<v8::Local<iv8::StringOneByte>> {
	const auto* data = reinterpret_cast<const std::uint8_t*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromOneByte(isolate(), data, v8::NewStringType::kNormal, size)).As<iv8::StringOneByte>();
	return js::referenceable_value{value};
}

auto accept_primitive::operator()(string_tag_of<char8_t> /*tag*/, visit_holder /*visit*/, std::basic_string_view<char8_t> subject) const
	-> js::referenceable_value<v8::Local<v8::String>> {
	const auto* data = reinterpret_cast<const char*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromUtf8(isolate(), data, v8::NewStringType::kNormal, size));
	return js::referenceable_value{value};
}

auto accept_primitive::operator()(string_tag_of<char16_t> /*tag*/, visit_holder /*visit*/, std::basic_string_view<char16_t> subject) const
	-> js::referenceable_value<v8::Local<iv8::StringTwoByte>> {
	const auto* data = reinterpret_cast<const std::uint16_t*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromTwoByte(isolate(), data, v8::NewStringType::kNormal, size)).As<iv8::StringTwoByte>();
	return js::referenceable_value{value};
}

// bigint (why does `NewFromWords` need a context?)
auto accept_value::operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, std::int64_t subject) const
	-> js::referenceable_value<v8::Local<iv8::BigInt64>> {
	auto value = v8::BigInt::New(isolate(), subject).As<iv8::BigInt64>();
	return js::referenceable_value{value};
}

auto accept_value::operator()(bigint_tag_of<std::uint64_t> /*tag*/, visit_holder /*visit*/, std::uint64_t subject) const
	-> js::referenceable_value<v8::Local<iv8::BigIntU64>> {
	auto value = v8::BigInt::NewFromUnsigned(isolate(), subject).As<iv8::BigIntU64>();
	return js::referenceable_value{value};
}

auto accept_value::operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
	-> js::referenceable_value<v8::Local<iv8::BigIntWords>> {
	auto value = unmaybe(v8::BigInt::NewFromWords(context_, subject.sign_bit(), static_cast<int>(subject.size()), subject.data())).As<iv8::BigIntWords>();
	return js::referenceable_value{value};
}

// date
auto accept_value::operator()(date_tag /*tag*/, visit_holder /*visit*/, js_clock::time_point subject) const
	-> js::referenceable_value<v8::Local<v8::Date>> {
	auto value = unmaybe(v8::Date::New(context_, subject.time_since_epoch().count())).As<v8::Date>();
	return js::referenceable_value{value};
}

// error
auto accept_value::operator()(error_tag /*tag*/, visit_holder /*visit*/, const js::error_value& subject) const
	-> js::referenceable_value<v8::Local<v8::Object>> {
	auto message = js::transfer_in<v8::Local<v8::String>>(subject.message(), witness());
	auto error = v8::Local<v8::Value>{[ & ]() -> v8::Local<v8::Value> {
		switch (subject.name()) {
			// These functions don't need an isolate somehow?
			default:
				return v8::Exception::Error(message);
			case js::error_name::range_error:
				return v8::Exception::RangeError(message);
			case js::error_name::reference_error:
				return v8::Exception::ReferenceError(message);
			case js::error_name::syntax_error:
				return v8::Exception::SyntaxError(message);
			case js::error_name::type_error:
				return v8::Exception::TypeError(message);
		}
	}()};
	return js::referenceable_value{error.As<v8::Object>()};
}

// data blocks
auto accept_value::operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, js::array_buffer subject) const
	-> js::referenceable_value<v8::Local<v8::ArrayBuffer>> {
	auto byte_length = subject.size();
	auto backing_store = v8::ArrayBuffer::NewBackingStore(
		std::move(subject).acquire_ownership().release(),
		byte_length,
		[](void* data, std::size_t /*length*/, void* /*deleter_data*/) -> void {
			std::ignore = js::array_buffer::unique_pointer_type{reinterpret_cast<std::byte*>(data)};
		},
		nullptr
	);
	auto value = v8::ArrayBuffer::New(isolate(), std::move(backing_store));
	return js::referenceable_value{value};
}

auto accept_value::operator()(shared_array_buffer_tag /*tag*/, visit_holder /*visit*/, js::shared_array_buffer&& subject) const
	-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>> {
	auto byte_length = subject.size();
	auto backing_store = [ & ]() -> auto {
		if (byte_length == 0) {
			// v8 does not call the deleter if `byte_length` is zero. So the heap-allocated shared_ptr
			// trick does not work in that case.
			return v8::SharedArrayBuffer::NewBackingStore(nullptr, 0, nullptr, nullptr);
		} else {
			auto holder = std::make_unique<js::shared_array_buffer::shared_pointer_type>(std::move(subject).acquire_ownership());
			auto backing_store = v8::SharedArrayBuffer::NewBackingStore(
				holder->get(),
				byte_length,
				[](void* /*data*/, std::size_t /*length*/, void* param) -> void {
					delete static_cast<js::shared_array_buffer::shared_pointer_type*>(param);
				},
				holder.get()
			);
			std::ignore = holder.release();
			return backing_store;
		}
	}();
	auto value = v8::SharedArrayBuffer::New(isolate(), std::move(backing_store));
	return js::referenceable_value{value};
}

auto accept_value::operator()(shared_array_buffer_tag tag, visit_holder visit, const js::shared_array_buffer& subject) const
	-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>> {
	return (*this)(tag, visit, js::shared_array_buffer{subject});
}

} // namespace js::iv8
