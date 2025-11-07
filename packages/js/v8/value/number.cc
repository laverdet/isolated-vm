module;
#include <cstdint>
export module v8_js:number;
import :handle;
import isolated_js;
import v8;

namespace js::iv8 {

export class number : public v8::Local<v8::Number> {
	public:
		explicit number(v8::Local<v8::Number> handle) :
				v8::Local<v8::Number>{handle} {}

		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator int32_t() const;
		[[nodiscard]] explicit operator int64_t() const;
		[[nodiscard]] explicit operator uint32_t() const;
};

export class bigint_n : public v8::Local<v8::BigInt> {
	public:
		explicit bigint_n(v8::Local<v8::BigInt> handle) :
				v8::Local<v8::BigInt>{handle} {}

		[[nodiscard]] explicit operator bigint() const;
};

export class bigint_u64 : public v8::Local<v8::BigInt> {
	public:
		explicit bigint_u64(v8::Local<v8::BigInt> handle, uint64_t value) :
				v8::Local<v8::BigInt>{handle},
				value_{value} {}

		[[nodiscard]] explicit operator uint64_t() const;

	private:
		uint64_t value_;
};

// ---

number::operator double() const {
	return (*this)->Value();
}

number::operator int32_t() const {
	return this->As<v8::Int32>()->Value();
}

number::operator int64_t() const {
	return this->As<v8::Integer>()->Value();
}

number::operator uint32_t() const {
	return this->As<v8::Uint32>()->Value();
}

bigint_n::operator bigint() const {
	auto bigint = js::bigint{};
	bigint.resize_and_overwrite((*this)->WordCount(), [ & ](auto* words, auto length) {
		if (length > 0) {
			auto int_length = static_cast<int>(length);
			(*this)->ToWordsArray(&bigint.sign_bit(), &int_length, words);
		}
		return length;
	});
	return bigint;
}

bigint_u64::operator uint64_t() const {
	return value_;
}

} // namespace js::iv8
