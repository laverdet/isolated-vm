module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;
import v8_js;
import std;

namespace isolated_vm {
using namespace js;

// value_for_data_block
auto value_for_data_block::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsArrayBuffer()) {
		return value_typeof::array_buffer;
	} else {
		return value_typeof::shared_array_buffer;
	}
}

// bound_value_for_data_block
bound_value_for_data_block::operator std::span<std::byte>() const {
	return {data(), byte_length()};
}

bound_value_for_data_block::operator js::array_buffer() const {
	return js::array_buffer{std::span<std::byte>{*this}};
}

auto bound_value_for_data_block::byte_length() const -> std::size_t {
	auto handle = cast_in(value_of{*this}).As<v8::ArrayBuffer>();
	return handle->ByteLength();
}

auto bound_value_for_data_block::data() const -> std::byte* {
	auto handle = cast_in(value_of{*this}).As<v8::ArrayBuffer>();
	return static_cast<std::byte*>(handle->Data());
}

// value_for_array_buffer
auto value_for_array_buffer::make(const runtime_lock& lock, js::array_buffer buffer) -> value_of<array_buffer_tag> {
	return cast_out(js::transfer_in<v8::Local<v8::ArrayBuffer>>(std::move(buffer), unwrap_lock_witness(lock)));
}

// value_for_array_buffer_view
auto value_for_array_buffer_view::inspect() const -> value_typeof {
	auto subject = cast_in(*this);
	if (subject->IsUint8Array()) {
		return value_typeof::uint8_array;
	} else if (subject->IsDataView()) {
		return value_typeof::data_view;
	} else if (subject->IsUint8ClampedArray()) {
		return value_typeof::uint8_clamped_array;
	} else if (subject->IsInt8Array()) {
		return value_typeof::int8_array;
	} else if (subject->IsUint16Array()) {
		return value_typeof::uint16_array;
	} else if (subject->IsInt16Array()) {
		return value_typeof::int16_array;
	} else if (subject->IsUint32Array()) {
		return value_typeof::uint32_array;
	} else if (subject->IsInt32Array()) {
		return value_typeof::int32_array;
	} else if (subject->IsFloat16Array()) {
		return value_typeof::float16_array;
	} else if (subject->IsFloat32Array()) {
		return value_typeof::float32_array;
	} else if (subject->IsFloat64Array()) {
		return value_typeof::float64_array;
	} else if (subject->IsBigInt64Array()) {
		return value_typeof::bigint64_array;
	} else if (subject->IsBigUint64Array()) {
		return value_typeof::biguint64_array;
	} else {
		std::unreachable();
	}
}

// bound_value_for_typed_array_of<Type>
template <class Type>
auto bound_value_for_typed_array_of<Type>::buffer() const -> value_of<data_block_tag> {
	return cast_out(cast_in(value_of{*this})->Buffer().template As<iv8::DataBlock>());
}

template <class Type>
auto bound_value_for_typed_array_of<Type>::byte_offset() const -> std::size_t {
	return cast_in(value_of{*this})->ByteOffset();
}

template <class Type>
auto bound_value_for_typed_array_of<Type>::size() const -> std::size_t {
	if constexpr (type<Type> == type<void>) {
		return cast_in(value_of{*this})->ByteLength();
	} else {
		return cast_in(value_of{*this})->Length();
	}
}

template class bound_value_for_typed_array_of<double>;
template class bound_value_for_typed_array_of<float>;
template class bound_value_for_typed_array_of<js::float16_t>;
template class bound_value_for_typed_array_of<js::uint8_clamped_t>;
template class bound_value_for_typed_array_of<std::int16_t>;
template class bound_value_for_typed_array_of<std::int32_t>;
template class bound_value_for_typed_array_of<std::int64_t>;
template class bound_value_for_typed_array_of<std::int8_t>;
template class bound_value_for_typed_array_of<std::uint16_t>;
template class bound_value_for_typed_array_of<std::uint32_t>;
template class bound_value_for_typed_array_of<std::uint64_t>;
template class bound_value_for_typed_array_of<std::uint8_t>;
template class bound_value_for_typed_array_of<void>;

} // namespace isolated_vm
