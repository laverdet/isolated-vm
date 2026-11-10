module v8_js;
import auto_js;
import std;
import v8;

namespace js::iv8 {

// After 256 bytes externalized strings tend to beat internalized strings on pure transfer speed.
// String operations (split, JSON.parse, etc) tend suffer in all cases though. 1kb is a decent
// cutoff.
constexpr std::size_t external_string_threshold_bytes = 1'024;

// Holder for externalized strings
template <class Char>
struct external_string_resource_base;

template <>
struct external_string_resource_base<char> : v8::String::ExternalOneByteStringResource {
	protected:
		using data_type = const char*;
};

template <>
struct external_string_resource_base<char16_t> : v8::String::ExternalStringResource {
	protected:
		using data_type = const std::uint16_t*;
};

template <class Char>
class external_string_resource final : public external_string_resource_base<Char> {
	public:
		using typename external_string_resource_base<Char>::data_type;

		explicit external_string_resource(v8::Isolate* isolate, std::basic_string<Char>&& value) :
				value_{std::move(value)} {
			accounter_.Increase(isolate, bytes());
		}

		[[nodiscard]] auto data() const -> data_type final { return reinterpret_cast<data_type>(value_.data()); }
		[[nodiscard]] auto length() const -> std::size_t final { return value_.size(); }
		[[nodiscard]] auto EstimateMemoryUsage() const -> std::size_t final { return sizeof(*this) + bytes(); }
		auto Unaccount(v8::Isolate* isolate) -> void final { accounter_.Decrease(isolate, bytes()); }

	private:
		[[nodiscard]] auto bytes() const -> std::size_t { return value_.size() * sizeof(Char); }

		std::basic_string<Char> value_;
		v8::ExternalMemoryAccounter accounter_;
};

template <class Char>
auto should_externalize(const std::basic_string<Char>& subject) -> bool {
	auto meets_threshold = subject.size() * sizeof(Char) >= external_string_threshold_bytes;
	auto under_max_length = subject.size() <= static_cast<std::size_t>(v8::String::kMaxLength);
	return meets_threshold && under_max_length;
}

// null
auto accept_v8_primitive::operator()(null_tag /*tag*/, visit_holder /*visit*/, std::nullptr_t /*subject*/) const -> v8::Local<iv8::Null> {
	return v8::Null(isolate()).As<iv8::Null>();
}

// boolean
auto accept_v8_primitive::operator()(boolean_tag /*tag*/, visit_holder /*visit*/, bool subject) const -> v8::Local<v8::Boolean> {
	return v8::Boolean::New(isolate_, subject);
}

// number
auto accept_v8_primitive::operator()(number_tag_of<double> /*tag*/, visit_holder /*visit*/, double subject) const -> v8::Local<iv8::Double> {
	return v8::Number::New(isolate_, subject).As<iv8::Double>();
}

auto accept_v8_primitive::operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, std::int32_t subject) const -> v8::Local<v8::Int32> {
	return v8::Integer::New(isolate_, subject).As<v8::Int32>();
}

auto accept_v8_primitive::operator()(number_tag_of<std::uint32_t> /*tag*/, visit_holder /*visit*/, std::uint32_t subject) const -> v8::Local<v8::Uint32> {
	return v8::Integer::NewFromUnsigned(isolate_, subject).As<v8::Uint32>();
}

// string
auto accept_v8_primitive::operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::string_view subject) const
	-> js::referenceable_value<v8::Local<iv8::StringOneByte>> {
	const auto* data = reinterpret_cast<const std::uint8_t*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromOneByte(isolate(), data, v8::NewStringType::kNormal, size)).As<iv8::StringOneByte>();
	return js::referenceable_value{value};
}

auto accept_v8_primitive::operator()(string_tag_of<char8_t> /*tag*/, visit_holder /*visit*/, std::u8string_view subject) const
	-> js::referenceable_value<v8::Local<v8::String>> {
	const auto* data = reinterpret_cast<const char*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromUtf8(isolate(), data, v8::NewStringType::kNormal, size));
	return js::referenceable_value{value};
}

auto accept_v8_primitive::operator()(string_tag_of<char16_t> /*tag*/, visit_holder /*visit*/, std::u16string_view subject) const
	-> js::referenceable_value<v8::Local<iv8::StringTwoByte>> {
	const auto* data = reinterpret_cast<const std::uint16_t*>(subject.data());
	auto size = static_cast<int>(subject.size());
	auto value = unmaybe(v8::String::NewFromTwoByte(isolate(), data, v8::NewStringType::kNormal, size)).As<iv8::StringTwoByte>();
	return js::referenceable_value{value};
}

auto accept_v8_primitive::operator()(string_tag_of<char> tag, visit_holder visit, std::string&& subject) const
	-> js::referenceable_value<v8::Local<iv8::StringOneByte>> {
	if (should_externalize(subject)) {
		auto resource = std::make_unique<external_string_resource<char>>(isolate(), std::move(subject));
		auto value = unmaybe(v8::String::NewExternalOneByte(isolate(), resource.release())).As<iv8::StringOneByte>();
		return js::referenceable_value{value};
	} else {
		return (*this)(tag, visit, std::string_view{subject});
	}
}

auto accept_v8_primitive::operator()(string_tag_of<char16_t> tag, visit_holder visit, std::u16string&& subject) const
	-> js::referenceable_value<v8::Local<iv8::StringTwoByte>> {
	if (should_externalize(subject)) {
		auto resource = std::make_unique<external_string_resource<char16_t>>(isolate(), std::move(subject));
		auto value = unmaybe(v8::String::NewExternalTwoByte(isolate(), resource.release())).As<iv8::StringTwoByte>();
		return js::referenceable_value{value};
	} else {
		return (*this)(tag, visit, std::u16string_view{subject});
	}
}

// bigint (why does `NewFromWords` need a context?)
auto accept_v8_value::operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, std::int64_t subject) const
	-> js::referenceable_value<v8::Local<iv8::BigInt64>> {
	auto value = v8::BigInt::New(isolate(), subject).As<iv8::BigInt64>();
	return js::referenceable_value{value};
}

auto accept_v8_value::operator()(bigint_tag_of<std::uint64_t> /*tag*/, visit_holder /*visit*/, std::uint64_t subject) const
	-> js::referenceable_value<v8::Local<iv8::BigIntU64>> {
	auto value = v8::BigInt::NewFromUnsigned(isolate(), subject).As<iv8::BigIntU64>();
	return js::referenceable_value{value};
}

auto accept_v8_value::operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
	-> js::referenceable_value<v8::Local<iv8::BigIntWords>> {
	auto value = unmaybe(v8::BigInt::NewFromWords(context_, subject.sign_bit(), static_cast<int>(subject.size()), subject.data())).As<iv8::BigIntWords>();
	return js::referenceable_value{value};
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
auto accept_v8_value::operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
	-> js::referenceable_value<v8::Local<iv8::BigIntWords>> {
	return (*this)(tag, visit, static_cast<const js::bigint&>(subject));
}

// date
auto accept_v8_value::operator()(date_tag /*tag*/, visit_holder /*visit*/, js_clock::time_point subject) const
	-> js::referenceable_value<v8::Local<v8::Date>> {
	auto value = unmaybe(v8::Date::New(context_, subject.time_since_epoch().count())).As<v8::Date>();
	return js::referenceable_value{value};
}

// error
auto accept_v8_value::operator()(error_tag /*tag*/, visit_holder /*visit*/, const js::error_value& subject) const
	-> js::referenceable_value<v8::Local<v8::Object>> {
	auto message = js::transfer_in<v8::Local<v8::String>>(subject.message(), witness());
	auto error = v8::Local<v8::Value>{[ & ] -> v8::Local<v8::Value> {
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

// function
auto accept_v8_value::operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::FunctionTemplate> subject) const -> v8::Local<iv8::Function> {
	return unmaybe(subject->GetFunction(context_).As<iv8::Function>());
}

// data blocks
auto accept_v8_value::operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, js::array_buffer subject) const
	-> js::referenceable_value<v8::Local<v8::ArrayBuffer>> {
	auto byte_length = subject.byte_length();
	auto backing_store = [ & ] -> auto {
		// nb: v8 does not call the deleter when `byte_length` is zero.
		if (byte_length == 0) {
			return v8::ArrayBuffer::NewBackingStore(nullptr, 0, nullptr, nullptr);
		} else {
			auto holder = std::make_unique<js::array_buffer>(std::move(subject));
			auto backing_store = v8::ArrayBuffer::NewBackingStore(
				holder->data(),
				byte_length,
				[](void* /*data*/, std::size_t /*length*/, void* param) -> void {
					delete static_cast<js::array_buffer*>(param);
				},
				holder.get()
			);
			std::ignore = holder.release();
			return backing_store;
		}
	}();
	auto value = v8::ArrayBuffer::New(isolate(), std::move(backing_store));
	return js::referenceable_value{value};
}

auto accept_v8_value::operator()(shared_array_buffer_tag /*tag*/, visit_holder /*visit*/, js::shared_array_buffer&& subject) const
	-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>> {
	auto byte_length = subject.byte_length();
	auto backing_store = [ & ] -> auto {
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

auto accept_v8_value::operator()(shared_array_buffer_tag tag, visit_holder visit, const js::shared_array_buffer& subject) const
	-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>> {
	return (*this)(tag, visit, js::shared_array_buffer{subject});
}

} // namespace js::iv8
